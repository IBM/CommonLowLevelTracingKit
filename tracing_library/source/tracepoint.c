// Copyright (c) 2024, International Business Machines
// SPDX-License-Identifier: BSD-2-Clause-Patent

#include "CommonLowLevelTracingKit/tracing/tracing.h"

#include "arguments.h"
#include "tracebuffer.h"
#include "tracepoint.h"

#include "abstraction/error.h"
#include "abstraction/info.h"
#include "abstraction/memory.h"
#include "abstraction/optimization.h"

#if defined(__KERNEL__)
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/version.h>
#if LINUX_VERSION_CODE > (KERNEL_VERSION(5, 14, 0))
#include <linux/stdarg.h>
#else
#include <stdarg.h>
#endif
#define UINT16_MAX ((uint16_t)0xFFFF)
#else
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

static size_t strnlen_s(const char *str, size_t len)
{
	return (str != NULL) ? strnlen(str, len) : 0;
}

#endif

_CLLTK_PRAGMA_DIAG(push)
_CLLTK_PRAGMA_DIAG(ignored "-Wstack-protector")
void _clltk_static_tracepoint_with_args(_clltk_tracebuffer_handler_t *handler,
										const _clltk_file_offset_t in_file_offset,
										const char *const file, const uint32_t line,
										_clltk_argument_types_t *types, const char *const format,
										...)
{
	if (unlikely(false == _CLLTK_FILE_OFFSET_IS_STATIC(in_file_offset))) {
		ERROR_LOG("invalid in_file_offset(%lu) at %s:%d for %s", in_file_offset, file, line,
				  handler->definition.name);
		return;
	}

	traceentry_head_t traceentry_head = {
		.in_file_offset = in_file_offset & ((1ull << 48) - 1),
		.pid = info_get_process_id(),
		.tid = info_get_thread_id(),
		.timestamp_ns = info_get_timestamp_ns(),
	};

	size_t raw_entry_size = sizeof(traceentry_head);
	// open va_arg
	va_list args;
	va_start(args, format);

	if (unlikely(types->count > 10)) {
		ERROR_LOG("to much arguments (%d) in clltk tracepoint", types->count);
		return;
	}

	uint32_t arg_sizes[10] = {0};
	raw_entry_size += get_argument_sizes(format, arg_sizes, types, args);

	// create entry
	if (unlikely(raw_entry_size >= UINT16_MAX)) {
		ERROR_LOG("raw entry size (%ld) bigger than max size in %s:%d", raw_entry_size, file, line);
		return;
	}

	uint8_t *raw_entry_buffer;
	// Optimize: Only use heap for really large entries (>1KB instead of page_size/2)
	const bool raw_buffer_is_heap_allocated = (types->flex_size && raw_entry_size > 1024);
	if (unlikely(raw_buffer_is_heap_allocated)) {
		raw_entry_buffer = memory_heap_allocation(raw_entry_size);
	} else {
		stack_alloc(&raw_entry_buffer, raw_entry_size);
	}

	traceentry_head_t *const entry_buffer = (traceentry_head_t *)raw_entry_buffer;
	memcpy((void *)entry_buffer, (void *)&traceentry_head, sizeof(traceentry_head));

	get_arguments((void *)&entry_buffer->body, arg_sizes, types, args);

	// close va_arg
	va_end(args);

	// add to ringbuffer
	add_to_ringbuffer(handler, raw_entry_buffer, raw_entry_size);
	if (unlikely(raw_buffer_is_heap_allocated)) {
		memory_heap_free(raw_entry_buffer);
	}
}
#ifdef __KERNEL__
EXPORT_SYMBOL(_clltk_static_tracepoint_with_args);
#endif

void _clltk_static_tracepoint_with_dump(_clltk_tracebuffer_handler_t *handler,
										const _clltk_file_offset_t in_file_offset,
										const char *const file, const uint32_t line,
										const void *address, uint32_t size_in_bytes)
{
	if (unlikely(false == _CLLTK_FILE_OFFSET_IS_STATIC(in_file_offset))) {
		ERROR_LOG("invalid in_file_offset(%lu) at %s:%d for %s", in_file_offset, file, line,
				  handler->definition.name);
		return;
	}

	traceentry_head_t traceentry_head = {
		.in_file_offset = in_file_offset & ((1ull << 48) - 1),
		.pid = info_get_process_id(),
		.tid = info_get_thread_id(),
		.timestamp_ns = info_get_timestamp_ns(),
	};

	size_t raw_entry_size = sizeof(traceentry_head);
	raw_entry_size += size_in_bytes + 4;

	// create entry
	if (unlikely(raw_entry_size >= UINT16_MAX)) {
		ERROR_LOG("raw entry size (%ld) bigger than max size in %s:%d", raw_entry_size, file, line);
		return;
	}

	uint8_t *raw_entry_buffer;
	const bool raw_buffer_is_heap_allocated = (raw_entry_size > memory_get_page_size() / 4);
	if (unlikely(raw_buffer_is_heap_allocated)) {
		raw_entry_buffer = memory_heap_allocation(raw_entry_size);
	} else {
		stack_alloc(&raw_entry_buffer, raw_entry_size);
	}

	traceentry_head_t *const entry_buffer = (traceentry_head_t *)raw_entry_buffer;
	memcpy((void *)entry_buffer, (void *)&traceentry_head, sizeof(traceentry_head));

	_Static_assert(sizeof(size_in_bytes) == 4, "size of value of dump size must be 4");
	memcpy(&entry_buffer->body[0], &size_in_bytes, sizeof(size_in_bytes));
	memcpy(&entry_buffer->body[4], address, size_in_bytes);

	// add to ringbuffer
	add_to_ringbuffer(handler, raw_entry_buffer, raw_entry_size);

	if (unlikely(raw_buffer_is_heap_allocated)) {
		memory_heap_free(raw_entry_buffer);
	}
}
#ifdef __KERNEL__
EXPORT_SYMBOL(_clltk_static_tracepoint_with_dump);
#endif

void clltk_dynamic_tracepoint_execution(const char *name, const char *file, const size_t line,
										const uint32_t pid, const uint32_t tid, const char *format,
										...)
{
	traceentry_head_t traceentry_head = {
		.in_file_offset = _clltk_file_offset_dynamic,
		.pid = (pid != 0 ? pid : info_get_process_id()),
		.tid = (tid != 0 ? tid : info_get_thread_id()),
		.timestamp_ns = info_get_timestamp_ns(),
	};

	char *message;
	va_list args;
	va_start(args, format);
	const int print_rc = vasprintf(&message, format, args);
	va_end(args);
	if (print_rc < 0) {
		ERROR_LOG("print failed with (%d) in %s:%ld", print_rc, file, line);
		return;
	}

	// calculate entry size
	const size_t file_len = strnlen_s(file, CLLTK_MAX_FILENAME_SIZE) + 1;
	const size_t message_len = (size_t)print_rc + 1;
	const size_t line_len = sizeof(line);

	size_t raw_entry_size = sizeof(traceentry_head);
	raw_entry_size += file_len;
	raw_entry_size += line_len;
	raw_entry_size += (size_t)message_len;

	// create entry
	if (unlikely(raw_entry_size >= UINT16_MAX)) {
		ERROR_LOG("raw entry size (%ld) bigger than max size in %s:%ld", raw_entry_size, file,
				  line);
		memory_heap_free(message);
		return;
	}

	uint8_t *raw_entry_buffer;
	const bool raw_buffer_is_heap_allocated = (raw_entry_size > memory_get_page_size() / 4);
	if (unlikely(raw_buffer_is_heap_allocated)) {
		raw_entry_buffer = memory_heap_allocation(raw_entry_size);
	} else {
		stack_alloc(&raw_entry_buffer, raw_entry_size);
	}

	traceentry_head_t *const entry_buffer = (traceentry_head_t *)raw_entry_buffer;
	memcpy((void *)entry_buffer, (void *)&traceentry_head, sizeof(traceentry_head));
	char *pos = (char *)&entry_buffer->body;
	pos += (memcpy(pos, file, file_len), file_len);
	pos += (memcpy(pos, &line, line_len), line_len);
	pos += (memcpy(pos, message, message_len), message_len);

	// add to ringbuffer
	_clltk_tracebuffer_handler_t handler = {{name, 10 * 1024}, {NULL, NULL}, {NULL, 0}};
	_clltk_tracebuffer_init(&handler);
	add_to_ringbuffer(&handler, raw_entry_buffer, raw_entry_size);
	_clltk_tracebuffer_deinit(&handler);

	if (unlikely(raw_buffer_is_heap_allocated)) {
		memory_heap_free(raw_entry_buffer);
	}
	memory_heap_free(message);
}
#ifdef __KERNEL__
EXPORT_SYMBOL(clltk_dynamic_tracepoint_execution);
#endif

_CLLTK_PRAGMA_DIAG(pop)
