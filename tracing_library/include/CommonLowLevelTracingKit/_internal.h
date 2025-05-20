// Copyright (c) 2024, International Business Machines
// SPDX-License-Identifier: BSD-2-Clause-Patent

#ifndef _CLLTK_INTERNAL_H_
#define _CLLTK_INTERNAL_H_
// IWYU pragma: private, include "CommonLowLevelTracingKit/tracing.h"

#if defined(__KERNEL__)
#include <linux/types.h>
#else
#include <stddef.h>
#include <stdint.h>
#endif

#include "CommonLowLevelTracingKit/_arguments.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _clltk_tracebuffer_t _clltk_tracebuffer_t;
typedef uint64_t _clltk_file_offset_t;
enum {
	_clltk_file_offset_unset = 0x00,
	_clltk_file_offset_dynamic = 0x01,
	_clltk_file_offset_invalid = 0xFF,
};
#define _CLLTK_FILE_OFFSET_IS_STATIC(_OFFSET_) ((_OFFSET_) > 0xFF)

typedef struct _clltk_tracebuffer_handler_t _clltk_tracebuffer_handler_t;
struct _clltk_tracebuffer_handler_t {
	const struct {
		const char *const name;
		const size_t size;
	} definition;
	struct {
		const void *start;
		const void *stop;
		_clltk_file_offset_t file_offset;
	} meta;
	_clltk_tracebuffer_t *volatile tracebuffer;
};

_clltk_tracebuffer_t *_clltk_tracebuffer_init_handler(_clltk_tracebuffer_handler_t *buffer)
	__attribute__((used, visibility("default")));

void _clltk_tracebuffer_reset_handler(_clltk_tracebuffer_handler_t *buffer)
	__attribute__((used, visibility("default")));

_clltk_file_offset_t _clltk_tracebuffer_add_to_stack(_clltk_tracebuffer_handler_t *buffer,
													 const void *new_entry, uint32_t new_entry_size)
	__attribute__((nonnull(1), used, visibility("default")));

void _clltk_static_tracepoint_with_args(_clltk_tracebuffer_handler_t *buffer,
										const _clltk_file_offset_t in_file_offset,
										const char *const file, const uint32_t line,
										_clltk_argument_types_t *types, const char *const format,
										...)
	__attribute__((nonnull(1, 3, 5), format(printf, 6, 7), used, visibility("default")));

static inline _clltk_file_offset_t
_clltk_tracebuffer_get_in_file_offset(_clltk_tracebuffer_handler_t *buffer,
									  const void *const this_meta, const uint32_t this_meta_size)
{
	const uintptr_t elf_sec_start = (uintptr_t)buffer->meta.start;
	const uintptr_t elf_sec_stop = (uintptr_t)buffer->meta.stop;
	const uint32_t elf_sec_size = (uint32_t)(elf_sec_stop - elf_sec_start);
	const uintptr_t this_start = (uintptr_t)this_meta;
	const uintptr_t this_stop = (uintptr_t)this_meta + (uintptr_t)this_meta_size;

	if ((buffer->meta.file_offset == _clltk_file_offset_unset) && (elf_sec_size > 0))
		buffer->meta.file_offset =
			_clltk_tracebuffer_add_to_stack(buffer, (const void *)elf_sec_start, elf_sec_size);

	if (buffer->meta.file_offset == _clltk_file_offset_invalid)
		return _clltk_file_offset_invalid;

	if ((elf_sec_start <= this_start) && (this_stop <= elf_sec_stop)) {
		// if meta for this tracepoint is in section
		return buffer->meta.file_offset + (_clltk_file_offset_t)(this_start - elf_sec_start);
	} else {
		// if meta for this tracepoint is not in section
		// this will happen with template functions
		return _clltk_tracebuffer_add_to_stack(buffer, this_meta, this_meta_size);
	}
}

void _clltk_static_tracepoint_with_dump(_clltk_tracebuffer_handler_t *buffer,
										const _clltk_file_offset_t in_file_offset,
										const char *const file, const uint32_t line,
										const void *address, uint32_t size_in_bytes)
	__attribute__((nonnull(1), used, visibility("default")));

#ifdef __cplusplus
}
#endif

#endif
