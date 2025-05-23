// Copyright (c) 2024, International Business Machines
// SPDX-License-Identifier: BSD-2-Clause-Patent

#include "CommonLowLevelTracingKit/tracing.h"
#include "CommonLowLevelTracingKit/version.gen.h"

#include "ringbuffer/ringbuffer.h"
#include "tracebuffer.h"

#include "c-vector/vec.h"
#include "crc8/crc8.h"

#include "abstraction/error.h"
#include "abstraction/file.h"
#include "abstraction/memory.h"
#include "abstraction/sync.h"
#include "unique_stack/unique_stack.h"

#if defined(__KERNEL__)
#include <linux/module.h>
#include <linux/string.h>
#include <linux/types.h>

#define PRId64 "d"
#define PRIx64 "llx"
#define PRIu64 "llu"
#else
#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

static size_t strnlen_s(const char *str, size_t len)
{
	return (str != NULL) ? strnlen(str, len) : 0;
}

__attribute__((always_inline)) static inline size_t round_up(size_t value, size_t alignment)
{
	const size_t r = (value % alignment);
	if (r == 0)
		return value;
	else
		return value + (alignment - r);
}
#endif

_clltk_tracebuffer_t **tracebufferes = NULL;

static bool tracebuffer_handler_matcher(const _clltk_tracebuffer_t *const *const vector_entry,
										const char *const name)
{
	return name != NULL && 0 == strcmp((*vector_entry)->name, name);
}

__attribute__((returns_nonnull)) static file_t *
create_tracebuffer_file(const char *const name, const size_t name_length, const size_t size)
{
	// create tracebuffer file because it was not found
	file_header_t file_head = {.magic = {FILE_MAGIC}, .version = CLLTK_VERSION};

	const size_t file_header_size = sizeof(file_head);
	file_head.definition_section_offset = file_header_size;
	const size_t definition_section_size = sizeof(uint64_t) + name_length;

	file_head.ringbuffer_section_offset =
		round_up(file_head.definition_section_offset + definition_section_size, 8);
	const size_t ringbuffer_section_size =
		sizeof(sync_mutex_t) + sizeof(ringbuffer_head_t) + size + 1;

	file_head.stack_section_offset =
		round_up(file_head.ringbuffer_section_offset + ringbuffer_section_size, 8);
	const size_t stack_section_size = sizeof(sync_mutex_t) + sizeof(unique_stack_header_t);

	file_head.crc = crc8_continue(0, (const uint8_t *)&file_head, sizeof(file_head) - 1);

	// create file
	const size_t required_file_size = file_head.stack_section_offset + stack_section_size;
	file_t *temp_file = file_create_temp(name, required_file_size);

	// fill file
	memset(file_mmap_ptr(temp_file), 0, required_file_size);

	// write header
	file_pwrite(temp_file, &file_head, sizeof(file_head), 0);

	// write definition
	file_pwrite(temp_file, &name_length, sizeof(name_length),
				file_head.definition_section_offset); // definition body size
	file_pwrite(temp_file, name, name_length,
				file_head.definition_section_offset + sizeof(name_length)); // definition body

	void *const ringbuffer_ptr =
		(void *)((uint64_t)file_mmap_ptr(temp_file) + file_head.ringbuffer_section_offset);
	const size_t ringbuffer_area_size =
		file_head.stack_section_offset - file_head.ringbuffer_section_offset;
	ringbuffer_head_t *const ringbuffer = ringbuffer_init(ringbuffer_ptr, ringbuffer_area_size);
	if (ringbuffer == NULL) {
		ERROR_AND_EXIT("failed to create ringbuffer");
	}
	sync_memory_mutex_init(&ringbuffer->mutex);

	unique_stack_handler_t stack = unique_stack_init(temp_file, file_head.stack_section_offset);
	if (!unique_stack_valid(&stack)) {
		ERROR_AND_EXIT("failed to init stack");
	}
	sync_mutex_t *const stack_mutex =
		(void *)((uint64_t)file_mmap_ptr(temp_file) + file_head.stack_section_offset +
				 offsetof(unique_stack_header_t, mutex));
	sync_memory_mutex_init(stack_mutex);

	file_t *const file_file = file_temp_to_final(&temp_file);

	return file_file;
}

static _clltk_tracebuffer_t *tracebuffer_open(const char *const name, size_t size)
{
	if ((name == NULL)) {
		return NULL;
	}

	// check if already open
	const vector_entry_match_t match =
		vector_find(tracebufferes, tracebuffer_handler_matcher, name);
	if (match.found)
		return tracebufferes[match.position];

	// create tracebuffer handler
	_clltk_tracebuffer_t *const tracebuffer_handler =
		memory_heap_allocation(sizeof(*tracebuffer_handler));
	if (!tracebuffer_handler) {
		ERROR_AND_EXIT("failed to allocate space for tracebuffer_handler for %s", name);
	}
	memset(tracebuffer_handler, 0, sizeof(*tracebuffer_handler));
	const size_t name_length = strnlen_s(name, CLLTK_MAX_NAME_SIZE);
	tracebuffer_handler->name = memory_heap_allocation(name_length + 1);
	if (!tracebuffer_handler->name) {
		ERROR_AND_EXIT("failed to allocate space for name for %s", name);
	}
	memcpy(tracebuffer_handler->name, name, name_length);

	// try to get existing tracebuffer file
	file_t *fh = file_try_get(name);
	if (fh == NULL) {
		fh = create_tracebuffer_file(name, name_length, size);
	}

	// now tracebuffer file must exist
	tracebuffer_handler->file = fh;
	file_header_t file_head = {0};
	file_pread(fh, &file_head, sizeof(file_head), 0);

	static const uint64_t expected_file_magic[2] = {FILE_MAGIC};
	if (0 != memcmp(file_head.magic, expected_file_magic, sizeof(file_head.magic))) {
		ERROR_AND_EXIT("found invalid file magic (%" PRIu64 " %" PRIu64 ") in %s",
					   file_head.magic[0], file_head.magic[1], name);
	}
	if ((file_head.version & CLLTK_VERSION_MASK) != ((CLLTK_VERSION)&CLLTK_VERSION_MASK)) {
		ERROR_AND_EXIT("found incompatible file version (%" PRIu64 ") in %s", file_head.version,
					   name);
	}

	void *const ringbuffer_ptr = (void *)((uint64_t)file_mmap_ptr(tracebuffer_handler->file) +
										  file_head.ringbuffer_section_offset);
	tracebuffer_handler->ringbuffer = ringbuffer_open(ringbuffer_ptr);
	if (NULL == (tracebuffer_handler->ringbuffer)) {
		ERROR_AND_EXIT("failed to init ringbuffer");
	}
	tracebuffer_handler->ringbuffer_mutex = &tracebuffer_handler->ringbuffer->mutex;

	tracebuffer_handler->stack =
		unique_stack_open(tracebuffer_handler->file, file_head.stack_section_offset);
	if (!unique_stack_valid(&tracebuffer_handler->stack)) {
		ERROR_AND_EXIT("failed to init stack");
	}
	unique_stack_header_t *const stack_header =
		(void *)((uint64_t)file_mmap_ptr(tracebuffer_handler->file) +
				 file_head.ringbuffer_section_offset);
	tracebuffer_handler->stack_mutex = &stack_header->mutex;

	vector_add(&tracebufferes, tracebuffer_handler);

	return tracebuffer_handler;
}

void _clltk_tracebuffer_init(_clltk_tracebuffer_handler_t *buffer)
{
	SYNC_GLOBAL_LOCK(global_lock);
	if (tracebufferes == NULL) {
		tracebufferes = vector_create();
		if (tracebufferes == NULL) {
			ERROR_AND_EXIT("could not create vector for tracebuffers");
		}
	}

	if (buffer->runtime.tracebuffer == NULL) {
		buffer->runtime.tracebuffer =
			tracebuffer_open(buffer->definition.name, buffer->definition.size);
		if (buffer->runtime.tracebuffer == NULL) {
			ERROR_LOG("could not open tracebuffer");
			return;
		}
	}
	buffer->runtime.tracebuffer->used++;
}

void _clltk_tracebuffer_deinit(_clltk_tracebuffer_handler_t *buffer)
{
	SYNC_GLOBAL_LOCK(global_lock); // global lock while removing tracebuffer
	if (buffer->runtime.tracebuffer == NULL)
		return;

	buffer->runtime.tracebuffer->used--;
	if (buffer->runtime.tracebuffer->used > 0)
		return;

	const vector_entry_match_t match =
		vector_find(tracebufferes, tracebuffer_handler_matcher, buffer->definition.name);

	_clltk_tracebuffer_t *const tb = buffer->runtime.tracebuffer;
	buffer->runtime.tracebuffer = NULL;
	buffer->runtime.file_offset = 0;
	memory_heap_free(tb->name);
	tb->used = 0;
	tb->ringbuffer = NULL;
	tb->ringbuffer_mutex = NULL;
	tb->stack.valid = false;
	tb->stack.file = NULL;
	tb->stack.file_offset = 0;
	tb->stack_mutex = NULL;
	file_drop(&tb->file);
	memory_heap_free(tb);

	if (match.found)
		vector_remove(tracebufferes, match.position);
	buffer->runtime.tracebuffer = NULL;

	if (vector_size(tracebufferes) == 0) {
		vector_free(&tracebufferes);
		tracebufferes = NULL;
	}
}

_clltk_file_offset_t _clltk_tracebuffer_add_to_stack(_clltk_tracebuffer_handler_t *handler,
													 const void *new_entry, uint32_t new_entry_size)
{
	if (new_entry == NULL || new_entry_size == 0) {
		ERROR_LOG("tried to add empty entry to stack");
		return _clltk_file_offset_invalid;
	}

	_clltk_tracebuffer_t *const buffer = handler->runtime.tracebuffer;

	if (buffer == NULL) {
		ERROR_LOG("no tracebuffer set");
		return _clltk_file_offset_invalid;
	}

	SYNC_MEMORY_LOCK(lock, buffer->stack_mutex);
	if (!lock.locked) {
		ERROR_LOG("could not lock stack update. ERROR was: %s", lock.error_msg);
		return _clltk_file_offset_invalid;
	}
	return unique_stack_add(&buffer->stack, new_entry, new_entry_size);
}

void add_to_ringbuffer(_clltk_tracebuffer_handler_t *handler, const void *const entry, size_t size)
{
	_clltk_tracebuffer_t *const buffer = handler->runtime.tracebuffer;
	if (buffer == NULL) {
		ERROR_LOG("no tracebuffer set");
		return;
	}

	SYNC_MEMORY_LOCK(lock, buffer->ringbuffer_mutex);
	if (!lock.locked)
		ERROR_LOG("could not lock ringbuffer update. ERROR was: %s", lock.error_msg);
	else if (0 == ringbuffer_in(buffer->ringbuffer, entry, size))
		ERROR_LOG("ringbuffer in failed for add_to_ringbuffer");
	return;
}

void clltk_dynamic_tracebuffer_creation(const char *buffer_name, size_t size)
{
	_clltk_tracebuffer_handler_t buffer = {{buffer_name, size}, {NULL, NULL}, {NULL, 0}};
	_clltk_tracebuffer_init(&buffer);
	_clltk_tracebuffer_deinit(&buffer);
}
