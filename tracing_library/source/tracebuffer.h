// Copyright (c) 2024, International Business Machines
// SPDX-License-Identifier: BSD-2-Clause-Patent

#pragma once

#include "CommonLowLevelTracingKit/tracing.h"
#include "ringbuffer/ringbuffer.h"
#include "unique_stack/unique_stack.h"

#include "abstraction/file.h"
#include "abstraction/sync.h"

#if defined(__KERNEL__)
#include <linux/types.h>
#else
#include <stddef.h>
#include <stdint.h>
#endif

#define FILE_MAGIC 0x636172747e24233f, 0x0072656666756265 // "?#$~tracebuffer\0"

struct file_header_t;
typedef struct file_header_t file_header_t;
struct __attribute__((packed, aligned(8))) file_header_t {
	uint64_t magic[2];
	uint64_t version;
	uint64_t definition_section_offset;
	uint64_t ringbuffer_section_offset;
	uint64_t stack_section_offset;
	uint8_t _padding[7];
	uint8_t crc;
};

_CLLTK_STATIC_ASSERT(sizeof(file_header_t) == 56, "file_header_t size invalid");
_CLLTK_STATIC_ASSERT(_CLLTK_OFFSETOF(file_header_t, crc) == 55, "file_header_t layout invalid");

struct _clltk_tracebuffer_t;
typedef struct _clltk_tracebuffer_t _clltk_tracebuffer_t;
struct _clltk_tracebuffer_t {
	_Atomic uint64_t used;
	char *name;
	file_t *file;
	sync_mutex_t *ringbuffer_mutex;
	ringbuffer_head_t *ringbuffer;
	unique_stack_handler_t stack;
	sync_mutex_t *stack_mutex;
};

void add_to_ringbuffer(_clltk_tracebuffer_handler_t *_clltk_tracebuffer_handler_t,
					   const void *const entry, size_t size);