// Copyright (c) 2024, International Business Machines
// SPDX-License-Identifier: BSD-2-Clause-Patent

#pragma once
#ifdef __cplusplus
extern "C" {
#endif

#if defined(__KERNEL__)
#include <linux/types.h>
#else
#include <stddef.h>
#endif

/**
 * @brief file handler, different foreach abstraction
 */
typedef struct file_t file_t;

/**
 * @brief get file if file already exists.
 *
 * @return returns null if file not existing
 */
__attribute__((visibility("hidden"))) file_t *file_try_get(const char *name);

/**
 * @brief create a unique file with given size
 *
 * @return file_descriptor_t
 */
file_t *file_create_temp(const char *final_name, const size_t file_size)
	__attribute__((returns_nonnull));

/**
 * @brief renaming and automatically deleting old one if renaming failed.
 *
 * @param temp_file file to be renamed
 */
file_t *file_temp_to_final(file_t **const temp_file)
	__attribute__((warn_unused_result, returns_nonnull, nonnull));

/**
 * @brief get file size.
 *
 * @param fh
 * @return size_t
 */
size_t file_get_size(file_t *fh) __attribute__((nonnull));

/**
 * @brief writing data, with length size, from source to file destination at offset.
 * Without affecting current offset in file descriptor
 *
 * @param destination file descriptor to which source should be written
 * @param source pointer to data to be written to file
 * @param size number of bytes which should be written
 * @param offset the file offset from which on to write
 * @return size_t number of bytes which where written
 */
size_t file_pwrite(const file_t *destination, const void *source, size_t size, size_t offset)
	__attribute__((nonnull(1)));

/**
 * @brief reading data, with length size, from file source, at offset offset to destination.
 * Without affecting current offset in file descriptor
 *
 * @param source file descriptor from which data should be read
 * @param destination pointer to buffer to which data are written
 * @param size  number of bytes to be read
 * @return size_t number of bytes which where read
 * @param offset the file offset from which on to read
 * @return size_t number of bytes which where read
 */
size_t file_pread(const file_t *source, void *destination, size_t size, size_t offset)
	__attribute__((nonnull(1)));

/**
 * @brief
 * @param fh
 * @return void* pointer to mmapped space
 */
void *file_mmap_ptr(const file_t *fh) __attribute__((returns_nonnull, nonnull));

/**
 * @brief
 * @param fh
 * @return size of mmapped space
 */
size_t file_mmap_size(const file_t *fh) __attribute__((nonnull));

/**
 * @brief drop every resource related to this file
 */
void file_drop(file_t **const fh) __attribute__((nonnull));

#ifdef UNITTEST
/**
 * @brief reset file abstraction
 */
void file_reset();
#endif

#ifdef __cplusplus
}
#endif
