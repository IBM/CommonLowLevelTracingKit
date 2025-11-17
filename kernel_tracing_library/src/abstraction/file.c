// Copyright (c) 2024, International Business Machines
// SPDX-License-Identifier: BSD-2-Clause-Patent

#include "abstraction/file.h"
#include "abstraction/error.h"
#include "abstraction/info.h"
#include "abstraction/sync.h"

#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/namei.h>
#include <linux/pagemap.h>
#include <linux/printk.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/stat.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/version.h>
#include <linux/vmalloc.h>

#define EXTENSION "clltk_ktrace"

extern char *tracing_path;

struct file_t {
	struct list_head list;
	uint64_t used;
	struct file *file;
	struct {
		void *ptr;
		size_t size;
	} mmapped;
	struct {
		size_t size;
		char *str;
	} name, file_name;
};

struct save_lock {
	spinlock_t lock;
	unsigned long flags;
};

struct file_context {
	char *root;
	struct save_lock slock;
	unsigned long lock_flags;
	struct file_t open_files;
};

static struct file_context context = {
	.root = NULL,
	.open_files =
		{
			.used = 0,
			.file = NULL,
		},
};

__attribute__((returns_nonnull)) static struct save_lock *get_files_lock(void)
{
	spin_lock_irqsave(&context.slock.lock, context.slock.flags);
	return &context.slock;
}

__attribute__((nonnull)) static void cleanup_file_lock(struct save_lock **slock)
{
	spin_unlock_irqrestore(&(*slock)->lock, (*slock)->flags);
}

static void init_context(void)
{
	if (context.root != NULL)
		return;

	context.root = tracing_path;
	INIT_LIST_HEAD(&context.open_files.list);
	spin_lock_init(&context.slock.lock);
}

static struct file_t *find_file(const char *name)
{
	pr_debug("> find_file (%s)\n", name);
	struct file_t *entry, *tmp;

	list_for_each_entry_safe(entry, tmp, &context.open_files.list, list)
	{
		if (entry->name.str)
			if (strncmp(name, entry->name.str, PATH_MAX) == 0) {

				pr_debug("< found (%pSR)\n", entry);
				return entry;
			}
	}
	pr_debug("< not found\n");
	return NULL;
}

static void fill_file(struct file_t *file, size_t size)
{
	pr_debug("> fill_file file(%s) buffer(%s) \n", file->file_name.str, file->name.str);
	void *empty = kzalloc(PAGE_SIZE, GFP_KERNEL);
	if (empty == NULL) {
		kfree(file->file_name.str);
		kfree(file->name.str);
		kfree(file);
		ERROR_AND_EXIT("fail to alloc space for file fill");
	}
	while (file->file->f_pos < size) {
		kernel_write(file->file, empty, PAGE_SIZE, &file->file->f_pos);
	}

	file->file->f_pos = 0;
	size_t pagecount = 0;
	loff_t pos = 0;
	while (kernel_read(file->file, empty, PAGE_SIZE, &pos)) {
		pagecount++;
	};
	kfree(empty);
}

static void *mmap_file(struct file_t *file, size_t size)
{
	pr_debug("> mmap_file file(%s) buffer(%s) \n", file->file_name.str, file->name.str);
	// find out needed mapping size

	size_t pagecount = 0;
	loff_t pos = 0;
	void *empty = kzalloc(PAGE_SIZE, GFP_KERNEL);
	if (empty == NULL) {
		ERROR_AND_EXIT("could not allocate memory for file read");
	}
	while (kernel_read(file->file, empty, PAGE_SIZE, &pos)) {
		pagecount++;
	}
	kfree(empty);

	struct address_space *mapping = file->file->f_mapping;
	struct page **pages = kcalloc(pagecount, sizeof(struct page), GFP_KERNEL);
	if (pages == NULL) {
		ERROR_AND_EXIT("could not allocate memory to hold file mapping pages");
	}

	pr_debug("  mmap_file collect pages count(0x%lx)\n", pagecount);

	// collect all pages to map
	for (size_t i = 0; i < pagecount; i++) {
		pages[i] = find_get_page(mapping, i);
		pr_debug("  page(0x%lx)=(%pSR)\n", i, pages[i]);
	}

// create kernel mapping of file
#if LINUX_VERSION_CODE < (KERNEL_VERSION(5, 8, 0))
	void *mmap = vm_map_ram(pages, pagecount, -1, PAGE_KERNEL);
#else
	void *mmap = vm_map_ram(pages, pagecount, -1);
#endif
	if (mmap == NULL) {
		ERROR_AND_EXIT("could not remap file into kernel space");
	}

	pr_debug("< mapped (%pSR) \n", mmap);
	kfree(pages);
	return mmap;
}

static struct file *filep_openat(const char *dir, const char *file_name, int flags, umode_t mode)
{
	pr_debug("> filep_openat dir(%s) file(%s) flags(0x%x) mode(0x%x)\n", dir, file_name, flags,
			 mode);
	char *real_path = kasprintf(GFP_KERNEL, "%s/%s", dir, file_name);
	if (real_path == NULL) {
		return (void *)-ENOMEM;
	}
	struct file *f = filp_open(real_path, flags, mode);
	pr_debug("< open(%pSR) \n", f);
	return f;
}

struct file_t *file_try_get(const char *name)
{
	pr_debug("> file_try_get name(%s)\n", name);
	init_context();
	struct save_lock *lock SYNC_CLEANUP(cleanup_file_lock) = get_files_lock();

	// check file already open
	struct file_t *file = find_file(name);
	if (file != NULL) {
		file->used++;
		pr_debug("< return already open at (0x%pSR)\n", file);
		return file;
	}

	// add filename to file struct
	file = kzalloc(sizeof(struct file_t), GFP_KERNEL);
	if (file == NULL) {
		ERROR_AND_EXIT("fail to calloc for file struct");
	}

	file->name.str = kstrndup(name, PATH_MAX, GFP_KERNEL);
	if (file->name.str == NULL) {
		kfree(file);
		ERROR_AND_EXIT("fail to calloc for buffer name");
	}
	file->name.size = strnlen(file->name.str, PATH_MAX);

	file->file_name.str = kasprintf(GFP_KERNEL, "%s.%s", file->name.str, EXTENSION);
	if (file->file_name.str == NULL) {
		kfree(file->name.str);
		kfree(file);
		ERROR_AND_EXIT("failed to calloc for file name");
	}
	file->file_name.size = strnlen(file->file_name.str, PATH_MAX);

	// open file
	file->file = filep_openat(context.root, file->file_name.str, O_RDWR | O_SYNC, 0644);
	if (IS_ERR(file->file)) {
		pr_debug("< file does not exist %s", file->name.str);
		kfree(file->name.str);
		kfree(file->file_name.str);
		kfree(file);
		return NULL;
	}

	// file size
	file->mmapped.size = vfs_llseek(file->file, 0, SEEK_END);

	// mmap file
	file->mmapped.ptr = mmap_file(file, file->mmapped.size);
	if (file->mmapped.ptr == NULL) {
		kfree(file->name.str);
		kfree(file->file_name.str);
		kfree(file);
		ERROR_AND_EXIT("failed to mmap file");
	}

	file->used++;
	list_add(&file->list, &context.open_files.list);

	pr_debug("< return newly opened at (0x%pSR)\n", file);

	return file;
}

struct file_t *file_create_temp(const char *name, const size_t file_size)
{
	pr_debug("> file_create_temp name(%s) size(0x%lx)\n", name, file_size);
	const size_t file_size_aligned = PAGE_ALIGN(file_size);
	pr_debug(">> file_create_temp name(%s) size(0x%lx)\n", name, file_size_aligned);
	init_context();
	struct save_lock *lock SYNC_CLEANUP(cleanup_file_lock) = get_files_lock();

	struct file_t *file = kzalloc(sizeof(struct file), GFP_KERNEL);
	if (file == NULL) {
		ERROR_AND_EXIT("fail to alloc for file handler");
	}

	file->name.str = kstrndup(name, PATH_MAX, GFP_KERNEL);
	if (file->name.str == NULL) {
		kfree(file);

		ERROR_AND_EXIT("fail to calloc for buffer name");
	}
	file->name.size = strnlen(file->name.str, PATH_MAX);

	file->file_name.str = kasprintf(GFP_KERNEL, "%s.%s", file->name.str, EXTENSION);
	if (file->file_name.str == NULL) {
		kfree(file->name.str);
		kfree(file);
		ERROR_AND_EXIT("failed to calloc for file name");
	}
	file->file_name.size = strnlen(file->file_name.str, PATH_MAX);

	file->file =
		filep_openat(context.root, file->file_name.str, O_RDWR | O_CREAT | O_EXCL | O_SYNC, 0644);
	if (IS_ERR(file->file)) {
		kfree(file->file_name.str);
		kfree(file->name.str);
		kfree(file);
		ERROR_AND_EXIT("failed to open file(%lx)", PTR_ERR(file->file));
	}

	fill_file(file, file_size_aligned);
	barrier();

	file->mmapped.ptr = mmap_file(file, file_size_aligned);
	if (IS_ERR(file->mmapped.ptr)) {
		kfree(file->file_name.str);
		kfree(file->name.str);
		kfree(file);
		ERROR_AND_EXIT("fail to mmap file");
	}
	file->mmapped.size = file_size_aligned;

	file->used++;
	list_add(&file->list, &context.open_files.list);

	pr_debug("< file created and opened at(%pSR)", file);

	return file;
}

struct file_t *file_temp_to_final(struct file_t **const temp_file)
{
	return *temp_file;
}

size_t file_get_size(struct file_t *fh)
{
	return (size_t)fh->mmapped.size;
}

size_t file_pwrite(const struct file_t *destination, const void *source, size_t size, size_t offset)
{
	loff_t pos = offset;

	const ssize_t n = kernel_write(destination->file, source, size, &pos);
	if (n < 0) {
		ERROR_AND_EXIT("pwrite failed for %s", destination->file_name.str);
	}
	return (size_t)n;
}

size_t file_pread(const struct file_t *source, void *destination, size_t size, size_t offset)
{
	loff_t pos = offset;
	const ssize_t n = kernel_read(source->file, destination, size, &pos);
	if (n < 0) {
		ERROR_AND_EXIT("pwrite failed for %s ", source->file_name.str);
	}
	return (size_t)n;
}

void *file_mmap_ptr(const struct file_t *fh)
{
	return fh->mmapped.ptr;
}

size_t file_mmap_size(const struct file_t *fh)
{
	return fh->mmapped.size;
}

void file_drop(struct file_t **const fh)
{
	struct save_lock *lock SYNC_CLEANUP(cleanup_file_lock) = get_files_lock();
	// currently we never drop files
	// this will very likely never be implemented for kernel
}
