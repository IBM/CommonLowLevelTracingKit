// Copyright (c) 2024, International Business Machines
// SPDX-License-Identifier: BSD-2-Clause-Patent

#include "abstraction/file.h"
#include "abstraction/error.h"
#include "abstraction/info.h"
#include "abstraction/sync.h"

#include "c-vector/vec.h"

#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <limits.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include <dirent.h>
#include <unistd.h>

#include <inttypes.h>
#include <pthread.h>
#include <stdlib.h>

static size_t strnlen_s(const char *str, size_t len)
{
	return (str != NULL) ? strnlen(str, len) : 0;
}

#define EXTENSION "clltk_trace"

struct file_t {
	uint64_t used;
	int file_descriptor;
	struct {
		void *ptr;
		size_t size;
	} mmapped;
	struct {
		size_t size;
		char *str;
	} name, file_name;
};

static struct {
	const char *root;
	int root_dir;
	file_t **files;
	pthread_mutex_t files_lock;
	bool closed;
} context = {.files_lock = PTHREAD_MUTEX_INITIALIZER};

static void only_once_called(void)
{
	// enviornment variable for root path for tracebuffers
	context.root = getenv("CLLTK_TRACING_PATH");
	if (context.root == NULL) {
		static char cwd[PATH_MAX];
		if (getcwd(cwd, sizeof(cwd)) == NULL) {
			ERROR_AND_EXIT("failed to get root for tracing with not set env variable with %s",
						   strerror(errno));
		}
		context.root = cwd;
	}
	context.root_dir = context.root ? open(context.root, O_DIRECTORY) : AT_FDCWD;
	if (context.root_dir == -1) {
		ERROR_AND_EXIT("failed to get tracing path (%s) with %s", context.root, strerror(errno));
	}
}

static void init_context(void)
{
	static pthread_once_t once = PTHREAD_ONCE_INIT;
	const int rc = pthread_once(&once, only_once_called);
	if (rc != 0) {
		ERROR_AND_EXIT("pthread_once failed with %d", rc);
	}
}

static const uint32_t ALL_READ_AND_WRITE = S_IREAD | S_IWUSR | S_IRGRP | S_IWGRP;

__attribute__((returns_nonnull)) pthread_mutex_t *get_files_lock()
{
	init_context();
	const int rc = pthread_mutex_lock(&context.files_lock);
	if (rc != 0) {
		ERROR_AND_EXIT("failed to get files lock with %d", rc);
	}
	if (context.files == NULL)
		context.files = vector_create();
	return &context.files_lock;
}

__attribute__((nonnull)) void cleanup_file_lock(const pthread_mutex_t **const lock)
{
	pthread_mutex_t *mutex = (pthread_mutex_t *)*lock;
	const int rc = pthread_mutex_unlock(mutex);
	if (rc != 0) {
		ERROR_AND_EXIT("failed to get files release %d", rc);
	}
}

bool file_is_valid(const file_t *const fh)
{
	if (fh == NULL)
		return false;
	if (fh->file_descriptor <= 0)
		return false;
	if (fcntl(fh->file_descriptor, F_GETFD) == -1)
		return false;
	return true;
}

bool file_matcher(const file_t *const *const vector_entry, const char *const name)
{
	if (!vector_entry || !(*vector_entry) || !name)
		return false;
	const file_t *const file = *vector_entry;
	if (!file || !file->name.str)
		return false;
	return 0 == strncmp(file->name.str, name, file->name.size);
}

file_t *file_try_get(const char *name)
{
	init_context();
	const pthread_mutex_t *lock CLEANUP(cleanup_file_lock) = get_files_lock();

	// check if already open
	vector_entry_match_t match = vector_find(context.files, file_matcher, name);
	if (match.found) {
		file_t *const file = context.files[match.position];
		file->used++;
		return file;
	} else {
		const size_t name_length = strnlen_s(name, UINT16_MAX) + 1;
		file_t *const file = calloc(1, sizeof(file_t));
		if (!file) {
			ERROR_AND_EXIT("fail to calloc for file handler");
		}

		file->name.size = name_length;
		file->name.str = calloc(1, file->name.size);
		if (!file->name.str) {
			free(file);
			ERROR_AND_EXIT("fail to calloc for name");
		}
		snprintf(file->name.str, file->name.size, "%s", name);

		file->file_name.size = name_length + sizeof("./." EXTENSION) + 1;
		file->file_name.str = calloc(1, file->file_name.size);
		if (!file->file_name.str) {
			free(file->name.str);
			free(file);
			ERROR_AND_EXIT("fail to calloc for file name");
		}
		snprintf(file->file_name.str, file->file_name.size, "./%s." EXTENSION, name);

		file->file_descriptor =
			openat(context.root_dir, file->file_name.str, O_RDWR | O_SYNC, ALL_READ_AND_WRITE);
		if (file->file_descriptor <= 0) {
			free(file->file_name.str);
			free(file->name.str);
			free(file);
			return NULL;
		}

		file->used++;

		if (context.files == NULL)
			context.files = vector_create();

		vector_add(&context.files, file);

		const size_t file_size = file_get_size(file);
		file->mmapped.ptr =
			mmap(0, file_size, PROT_READ | PROT_WRITE, MAP_SHARED, file->file_descriptor, 0);
		if (file->mmapped.ptr == MAP_FAILED) {
			file->mmapped.ptr = NULL;
			ERROR_AND_EXIT("failed to mmap file %s with %s", file->file_name.str, strerror(errno));
		}
		file->mmapped.size = file_size;

		return file;
	}
}

file_t *file_create_temp(const char *final_name, const size_t file_size)
{
	init_context();

	// create file
	const uint64_t unique_id = info_get_timestamp_ns();
	file_t *const file = calloc(1, sizeof(file_t));
	if (!file) {
		ERROR_AND_EXIT("fail to calloc for file handler");
	}
	file->used++;

	file->name.size = (final_name ? strnlen_s(final_name, PATH_MAX) : 0) + 32 + 1;
	file->name.str = calloc(1, file->name.size);
	if (!file->name.str) {
		free(file);
		ERROR_AND_EXIT("fail to calloc for name");
	}
	snprintf(file->name.str, file->name.size, "%s~%" PRIX64, final_name, unique_id);

	file->file_name.size = file->name.size + sizeof("./." EXTENSION);
	file->file_name.str = calloc(1, file->file_name.size);
	if (!file->file_name.str) {
		free(file->name.str);
		free(file);
		ERROR_AND_EXIT("fail to calloc for file name");
	}
	snprintf(file->file_name.str, file->file_name.size, "./%s." EXTENSION, file->name.str);

	{ // add temp file to files vector
		const pthread_mutex_t *lock CLEANUP(cleanup_file_lock) = get_files_lock();
		if (context.files == NULL)
			context.files = vector_create();
		vector_add(&context.files, file);
	}

	file->file_descriptor = openat(context.root_dir, file->file_name.str,
								   O_RDWR | O_CREAT | O_EXCL | O_SYNC, ALL_READ_AND_WRITE);
	if (file->file_descriptor < 0) {
		ERROR_AND_EXIT("fail to openat temp file %s with %s", file->file_name.str, strerror(errno));
	}

	// write \0 to last byte in file to set file size
	if (1 != pwrite(file->file_descriptor, (void *)&"\0", 1, (off_t)(file_size - 1))) {
		ERROR_AND_EXIT("fail to write to last byte in temp file %s with %s", file->file_name.str,
					   strerror(errno));
	}

	file->mmapped.ptr =
		mmap(0, file_size, PROT_READ | PROT_WRITE, MAP_SHARED, file->file_descriptor, 0);
	if (file->mmapped.ptr == MAP_FAILED) {
		file->mmapped.ptr = NULL;
		ERROR_AND_EXIT("fail to mmap temp file %s with %s", file->file_name.str, strerror(errno));
	}

	if (madvise(file->mmapped.ptr, file_size, MADV_DODUMP) == -1) {
		ERROR_AND_EXIT("fail to madvise temp file %s with %s", file->file_name.str,
					   strerror(errno));
	}

	file->mmapped.size = file_size;

	return file;
}

void file_drop(file_t **const file)
{
	if (!(*file)) {
		ERROR_AND_EXIT("tried to file_drop for null pointer");
	}

	file_t *const fh = *file;

	const pthread_mutex_t *lock CLEANUP(cleanup_file_lock) = get_files_lock();
	const vector_entry_match_t match = vector_find(context.files, file_matcher, fh->name.str);
	fh->used--;
	if (fh->used > 0)
		return;

	(*file) = NULL;

	((fh->mmapped.ptr != NULL) ? munmap(fh->mmapped.ptr, fh->mmapped.size) : 0);
	((fh->file_descriptor > 0) ? close(fh->file_descriptor) : 0);
	fh->file_descriptor = 0;
	((fh->file_name.str != NULL) ? free(fh->file_name.str) : 0);
	fh->file_name.str = 0;
	fh->file_name.size = 0;
	((fh->name.str != NULL) ? free(fh->name.str) : 0);
	fh->name.str = 0;
	fh->name.size = 0;

	free(fh);

	if (context.files && match.found) {
		vector_remove(context.files, match.position);
	}

	if (context.files != NULL && vector_size(context.files) == 0) {
		vector_free(&context.files);
	}

	return;
}

size_t file_get_size(file_t *fh)
{
	struct stat status = {0};
	if (fstat(fh->file_descriptor, &status)) {
		ERROR_AND_EXIT("failed to get file size for %s with %s", fh->file_name.str,
					   strerror(errno));
	}
	return (size_t)status.st_size;
}

size_t file_pwrite(const file_t *destination, const void *source, size_t size, size_t offset)
{
	const ssize_t n = pwrite(destination->file_descriptor, source, size, (ssize_t)offset);
	if (n < 0 || (size_t)n != size) {
		ERROR_AND_EXIT("pwrite failed for %s with %s", destination->file_name.str, strerror(errno));
	}
	return (size_t)n;
}

size_t file_pread(const file_t *source, void *destination, size_t size, size_t offset)
{
	const ssize_t n = pread(source->file_descriptor, destination, size, (ssize_t)offset);
	if (n < 0 || (size_t)n != size) {
		ERROR_AND_EXIT("pread failed for %s with %s", source->file_name.str, strerror(errno));
	}
	return (size_t)n;
}

file_t *file_temp_to_final(file_t **temp_file)
{
	file_t *old_file = *temp_file;
	if (old_file->mmapped.ptr) {
		if (munmap(old_file->mmapped.ptr, old_file->mmapped.size)) {
			ERROR_AND_EXIT("failed to close file for %s with %s", old_file->file_name.str,
						   strerror(errno));
		}
		old_file->mmapped.ptr = NULL;
		old_file->mmapped.size = 0;
	}
	if (old_file->file_descriptor > 0) {
		if (close(old_file->file_descriptor)) {
			ERROR_AND_EXIT("failed to close file for %s with %s", old_file->file_name.str,
						   strerror(errno));
		}
		old_file->file_descriptor = 0;
	}

	const char *const sep =
		(old_file->name.str) ? strchr(old_file->name.str, '~') : (old_file->name.str);
	const int name_len = (int)(sep - old_file->name.str);
	char name[PATH_MAX];
	snprintf(name, sizeof(name), "%.*s", name_len, old_file->name.str);

	char file_name[PATH_MAX];
	snprintf(file_name, sizeof(file_name), "./%.*s." EXTENSION, name_len, name);

	// rename file to final name
	int rename_return_code = 0;
	rename_return_code = renameat2(context.root_dir, old_file->file_name.str, context.root_dir,
								   file_name, RENAME_NOREPLACE);
	if (rename_return_code) {
		rename_return_code = errno;
		if (rename_return_code == EEXIST) {
			// remove temp_file if rename failed, than some one else was faster
			unlinkat(context.root_dir, old_file->file_name.str, 0);
			// care if unlink failed, because final tracebuffer file already exists
		} else {
			ERROR_AND_EXIT("renameing failed with \"%s\"(%d) for file %s at %s",
						   strerror(rename_return_code), rename_return_code,
						   old_file->file_name.str, context.root);
		}
	}

	// the file_file should now exist
	file_t *const final_file = file_try_get(name);

	file_drop(temp_file);

	return final_file;
}

void *file_mmap_ptr(const file_t *fh)
{
	return fh->mmapped.ptr;
}

size_t file_mmap_size(const file_t *fh)
{
	return fh->mmapped.size;
}

void file_reset(void)
{
	init_context();
	const char *const root = context.root ? context.root : ".";
	DIR *directory = opendir(root);
	struct dirent *iterator = NULL;

	// loop over all files in path
	while ((iterator = readdir(directory)) != NULL) {
		if (iterator->d_type != DT_REG)
			continue;
		const char *name = iterator->d_name;
		const char *ending = strrchr(name, '.');
		if (ending && !strcmp(ending, ".clltk_trace")) {
			char path[PATH_MAX];
			snprintf(path, sizeof(path), "%s/%s", root, name);
			if (0 != unlink(path))
				ERROR_AND_EXIT("remove %s at %s failed", name, root);
		}
	}
	closedir(directory);
}
