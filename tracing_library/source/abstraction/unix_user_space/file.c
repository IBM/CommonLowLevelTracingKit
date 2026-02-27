// Copyright (c) 2024, International Business Machines
// SPDX-License-Identifier: BSD-2-Clause-Patent

#include "abstraction/file.h"
#include "abstraction/error.h"
#include "abstraction/info.h"
#include "abstraction/sync.h"

#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <sys/mman.h>
#include <sys/stat.h>

#include <dirent.h>
#include <limits.h>
#include <unistd.h>

#include <inttypes.h>
#include <pthread.h>
#include <stdlib.h>
#include <sys/queue.h>

#define EXTENSION ".clltk_trace"

struct file_t {
	uint64_t used;
	int file_descriptor;
	char *name;
	char *path;
	struct {
		void *ptr;
		size_t size;
	} mmapped;
	SLIST_ENTRY(file_t) files;
};

static struct {
	SLIST_HEAD(files, file_t) files;
	pthread_mutex_t files_lock;
} context = {
	.files_lock = PTHREAD_MUTEX_INITIALIZER, //
	.files = SLIST_HEAD_INITIALIZER()		 //
};

// Path set via clltk_set_tracing_path() API - takes priority over environment variable
static char api_root_path[PATH_MAX] = {0};
static pthread_mutex_t api_root_path_lock = PTHREAD_MUTEX_INITIALIZER;

void clltk_set_tracing_path(const char *path)
{
	if (path == NULL || path[0] == '\0') {
		return;
	}

	if (pthread_mutex_lock(&api_root_path_lock) != 0) {
		ERROR_AND_EXIT("failed to acquire api_root_path lock");
	}

	strncpy(api_root_path, path, PATH_MAX - 1);
	api_root_path[PATH_MAX - 1] = '\0';

	if (pthread_mutex_unlock(&api_root_path_lock) != 0) {
		ERROR_AND_EXIT("failed to release api_root_path lock");
	}
}

static const char *get_root_path(void)
{
	// 1. Check if path was set via API (highest priority)
	if (api_root_path[0] != '\0') {
		return api_root_path;
	}

	// 2. Check environment variable or fall back to cwd (cached)
	static char *root_path = NULL;
	if (root_path)
		return root_path;

	static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
	if (pthread_mutex_lock(&lock) != 0)
		ERROR_AND_EXIT("failed to acquire lock");

	root_path = getenv("CLLTK_TRACING_PATH");
	if (root_path == NULL) {
		static char cwd[PATH_MAX];
		if (getcwd(cwd, sizeof(cwd)) == NULL) {
			ERROR_AND_EXIT("failed to get root for tracing with not set env variable with %s",
						   strerror(errno));
		}
		root_path = cwd;
	}
	if (pthread_mutex_unlock(&lock) != 0)
		ERROR_AND_EXIT("failed to release lock");
	return root_path;
}

static const uint32_t ALL_READ_AND_WRITE = S_IREAD | S_IWUSR | S_IRGRP | S_IWGRP;

static __attribute__((returns_nonnull)) pthread_mutex_t *get_files_lock()
{
	const int rc = pthread_mutex_lock(&context.files_lock);
	if (rc != 0) {
		ERROR_AND_EXIT("failed to get files lock with %d", rc);
	}
	return &context.files_lock;
}

static __attribute__((nonnull)) void cleanup_file_lock(const pthread_mutex_t **const lock)
{
	pthread_mutex_t *mutex = (pthread_mutex_t *)*lock;
	const int rc = pthread_mutex_unlock(mutex);
	if (rc != 0) {
		ERROR_AND_EXIT("failed to get files release %d", rc);
	}
}

static bool file_matcher(const file_t *const file, const char *const name)
{
	if (!file || !file->name || !name)
		return false;
	return 0 == strcmp(file->name, name);
}

static file_t *find_file(const char *const name)
{
	file_t *file = NULL;
	SLIST_FOREACH(file, &context.files, files)
	{
		if (file && file_matcher(file, name)) {
			return file;
		}
	}
	return NULL;
}

file_t *file_try_get(const char *name)
{
	const pthread_mutex_t *lock SYNC_CLEANUP(cleanup_file_lock) = get_files_lock();

	{ // check if already open
		file_t *const file = find_file(name);
		if (file) {
			file->used++;
			return file;
		}
	}
	file_t *const file = calloc(1, sizeof(file_t));
	if (!file) {
		ERROR_AND_EXIT("fail to calloc for file handler");
	}

	if (asprintf(&file->name, "%s", name) < 0) {
		free(file);
		ERROR_AND_EXIT("fail print name");
	}

	if (asprintf(&file->path, "%s/%s" EXTENSION, get_root_path(), name) < 0) {
		free(file->name);
		free(file);
		ERROR_AND_EXIT("fail to print file name");
	}

	file->file_descriptor = open(file->path, O_RDWR | O_SYNC, ALL_READ_AND_WRITE);
	if (file->file_descriptor < 0) {
		free(file->path);
		free(file->name);
		free(file);
		return NULL;
	}

	const size_t file_size = file_get_size(file);
	file->mmapped.ptr =
		mmap(0, file_size, PROT_READ | PROT_WRITE, MAP_SHARED, file->file_descriptor, 0);
	if (file->mmapped.ptr == MAP_FAILED) {
		file->mmapped.ptr = NULL;
		ERROR_AND_EXIT("failed to mmap file %s with %s", file->path, strerror(errno));
	}
	file->mmapped.size = file_size;

	file->used = 1;
	SLIST_INSERT_HEAD(&context.files, file, files);
	return file;
}

file_t *file_create_temp(const char *final_name, const size_t file_size)
{
	// create file
	const uint64_t unique_id = info_get_timestamp_ns();
	file_t *const file = calloc(1, sizeof(file_t));
	if (!file) {
		ERROR_AND_EXIT("fail to calloc for file handler");
	}
	file->used = 1;

	if (asprintf(&file->name, "%s~%" PRIX64, final_name, unique_id) < 0) {
		free(file);
		ERROR_AND_EXIT("fail to print tmp name");
	}

	if (asprintf(&file->path, "%s/%s" EXTENSION, get_root_path(), file->name) < 0) {
		free(file->name);
		free(file);
		ERROR_AND_EXIT("fail to calloc for file name");
	}

	{ // add temp file to files vector
		const pthread_mutex_t *lock SYNC_CLEANUP(cleanup_file_lock) = get_files_lock();
		SLIST_INSERT_HEAD(&context.files, file, files);
	}

	file->file_descriptor =
		open(file->path, O_RDWR | O_CREAT | O_EXCL | O_SYNC, ALL_READ_AND_WRITE);
	if (file->file_descriptor < 0) {
		ERROR_AND_EXIT("fail to openat temp file %s with %s", file->path, strerror(errno));
	}

	// write \0 to last byte in file to set file size
	if (1 != pwrite(file->file_descriptor, (void *)&"\0", 1, (off_t)(file_size - 1))) {
		ERROR_AND_EXIT("fail to write to last byte in temp file %s with %s", file->path,
					   strerror(errno));
	}

	file->mmapped.ptr =
		mmap(0, file_size, PROT_READ | PROT_WRITE, MAP_SHARED, file->file_descriptor, 0);
	if (file->mmapped.ptr == MAP_FAILED) {
		file->mmapped.ptr = NULL;
		ERROR_AND_EXIT("fail to mmap temp file %s with %s", file->path, strerror(errno));
	}

#ifdef __linux__
	if (madvise(file->mmapped.ptr, file_size, MADV_DODUMP) == -1) {
		ERROR_AND_EXIT("fail to madvise temp file %s with %s", file->path, strerror(errno));
	}
#endif

	file->mmapped.size = file_size;

	return file;
}

void file_drop(file_t **const file)
{
	if (!(*file)) {
		ERROR_AND_EXIT("tried to file_drop for null pointer");
	}

	file_t *const fh = *file;

	const pthread_mutex_t *lock SYNC_CLEANUP(cleanup_file_lock) = get_files_lock();
	file_t *const match = find_file(fh->name);
	if (match && ((--match->used) > 0)) {
		return;
	}

	(*file) = NULL;

	if (fh->mmapped.ptr != NULL) {
		if (munmap(fh->mmapped.ptr, fh->mmapped.size) != 0) {
			ERROR_LOG("failed to munmap file %s: %s", fh->path, strerror(errno));
		}
	}
	if (fh->file_descriptor > 0) {
		if (close(fh->file_descriptor) != 0) {
			ERROR_LOG("failed to close file %s: %s", fh->path, strerror(errno));
		}
	}
	fh->file_descriptor = 0;
	((fh->path != NULL) ? free(fh->path) : 0);
	fh->path = 0;
	((fh->name != NULL) ? free(fh->name) : 0);
	fh->name = 0;

	if (match) {
		SLIST_REMOVE(&context.files, match, file_t, files);
	}

	free(fh);
}

size_t file_get_size(file_t *fh)
{
	struct stat status = {0};
	if (fstat(fh->file_descriptor, &status)) {
		ERROR_AND_EXIT("failed to get file size for %s with %s", fh->path, strerror(errno));
	}
	return (size_t)status.st_size;
}

size_t file_pwrite(const file_t *destination, const void *source, size_t size, size_t offset)
{
	const ssize_t n = pwrite(destination->file_descriptor, source, size, (ssize_t)offset);
	if (n < 0 || (size_t)n != size) {
		ERROR_AND_EXIT("pwrite failed for %s with %s", destination->path, strerror(errno));
	}
	return (size_t)n;
}

size_t file_pread(const file_t *source, void *destination, size_t size, size_t offset)
{
	const ssize_t n = pread(source->file_descriptor, destination, size, (ssize_t)offset);
	if (n < 0 || (size_t)n != size) {
		ERROR_AND_EXIT("pread failed for %s with %s", source->path, strerror(errno));
	}
	return (size_t)n;
}

file_t *file_temp_to_final(file_t **temp_file)
{
	file_t *old_file = *temp_file;
	if (old_file->mmapped.ptr) {
		if (munmap(old_file->mmapped.ptr, old_file->mmapped.size)) {
			ERROR_AND_EXIT("failed to close file for %s with %s", old_file->path, strerror(errno));
		}
		old_file->mmapped.ptr = NULL;
		old_file->mmapped.size = 0;
	}
	if (old_file->file_descriptor > 0) {
		if (close(old_file->file_descriptor)) {
			ERROR_AND_EXIT("failed to close file for %s with %s", old_file->path, strerror(errno));
		}
		old_file->file_descriptor = 0;
	}

	const char *const sep = (old_file->name) ? strchr(old_file->name, '~') : (old_file->name);
	const int name_len = (int)(sep - old_file->name);
	char name[PATH_MAX];
	snprintf(name, sizeof(name), "%.*s", name_len, old_file->name);

	char path[PATH_MAX];
	snprintf(path, sizeof(path), "%s/%.*s" EXTENSION, get_root_path(), name_len, name);

	// rename file to final name
	if (linkat((int)AT_FDCWD, old_file->path, (int)AT_FDCWD, path, 0) == -1) {
		if (errno != EEXIST) {
			ERROR_AND_EXIT("renameing failed with \"%s\"(%d) for file %s to %s", strerror(errno),
						   errno, old_file->path, path);
		}
	}

	// the file_file should now exist
	file_t *const final_file = file_try_get(name);

	if (unlink(old_file->path) != 0) {
		ERROR_LOG("failed to unlink temp file %s: %s", old_file->path, strerror(errno));
	}
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
	const char *const root = get_root_path();
	DIR *directory = opendir(root);
	if (directory == NULL) {
		ERROR_LOG("failed to open directory %s: %s", root, strerror(errno));
		return;
	}
	struct dirent *iterator = NULL;
	struct stat file_stat;
	// loop over all files in path
	while ((iterator = readdir(directory)) != NULL) {
		if (fstatat(dirfd(directory), iterator->d_name, &file_stat, AT_SYMLINK_NOFOLLOW) == 0) {
			if (S_ISDIR(file_stat.st_mode))
				continue;
			const char *name = iterator->d_name;
			const char *ending = strrchr(name, '.');
			if (ending && !strcmp(ending, EXTENSION)) {
				char path[PATH_MAX];
				snprintf(path, sizeof(path), "%s/%s", root, name);
				if (0 != unlink(path))
					ERROR_AND_EXIT("remove %s at %s failed", name, root);
			}
		}
	}
	closedir(directory);
}
