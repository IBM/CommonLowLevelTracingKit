// Copyright (c) 2024, International Business Machines
// SPDX-License-Identifier: BSD-2-Clause-Patent

#include "unique_stack.h"

#include "abstraction/memory.h"
#include "crc8/crc8.h"
#include "md5/md5.h"

#if defined(__KERNEL__)
#include <linux/string.h>
#include <linux/types.h>
#else
#include <stddef.h>
#include <string.h>
#endif

#define RETURN_IF_INVALID(_FH_, ...) \
	if (!unique_stack_valid(_FH_))   \
	return __VA_ARGS__

static inline uint64_t body_offset(unique_stack_handler_t *fh)
{
	return fh->file_offset + sizeof(unique_stack_header_t);
}

bool unique_stack_valid(const unique_stack_handler_t *handler)
{
	return (handler != NULL) && (handler->valid);
}

static unique_stack_handler_t _unique_stack_open(file_t *fh, uint64_t file_offset, bool init)
{
	unique_stack_handler_t handler = {0};
	if (fh == NULL)
		return handler;

	handler.file = fh;
	handler.file_offset = file_offset;
	if (init) {
		unique_stack_header_t stack_header = {.version = 1};
		file_pwrite(fh, &stack_header, sizeof(stack_header), file_offset);
	}

	handler.valid = true;
	return handler;
}

unique_stack_handler_t unique_stack_init(file_t *fh, uint64_t file_offset)
{
	return _unique_stack_open(fh, file_offset, true);
}
unique_stack_handler_t unique_stack_open(file_t *fh, uint64_t file_offset)
{
	return _unique_stack_open(fh, file_offset, false);
}

void unique_stack_close(unique_stack_handler_t *handler)
{
	RETURN_IF_INVALID(handler);

	handler->valid = 0;
	handler->file = NULL;
	handler->file_offset = 0;
}

static uint64_t check_if_already_in_file(unique_stack_handler_t *handler, __uint128_t md5_hash)
{
	uint64_t offset_in_stack_body = 0;
	unique_stack_header_t header = {0};
	file_pread(handler->file, &header, sizeof(header), handler->file_offset);
	while (offset_in_stack_body < header.body_size) {
		// get next entry head
		entry_head_t head = {0};
		file_pread(handler->file, (void *)&head, sizeof(head),
				   body_offset(handler) + offset_in_stack_body);

		// check if the same
		if (head.md5_hash == md5_hash)
			return body_offset(handler) + offset_in_stack_body + sizeof(head);

		// increment to next entry
		offset_in_stack_body += sizeof(head) + head.body_size;
	}

	return 0; // if searched to whole stack without any finding
}

static __uint128_t hash_function(const void *body, uint32_t size)
{
	MD5Context context = {0};
	md5Init(&context);
	md5Update(&context, (uint8_t *)&size, sizeof(size));
	md5Update(&context, body, size);
	md5Finalize(&context);
	__uint128_t md5_hash = 0;
	memcpy(&md5_hash, context.digest, sizeof(md5_hash));
	return md5_hash;
}

uint64_t unique_stack_add(unique_stack_handler_t *handler, const void *body, uint32_t size)
{
	RETURN_IF_INVALID(handler, 0);

	const __uint128_t md5_hash = hash_function(body, size);
	uint64_t file_offset = check_if_already_in_file(handler, md5_hash);
	if (file_offset > 0) {
		// already in stack
		return file_offset;
	}
	// not in stack, so add this
	entry_head_t entry_head = {
		.md5_hash = md5_hash,
		.body_size = size,
	};
	entry_head.crc = crc8_continue(0, (const uint8_t *)&entry_head, sizeof(entry_head) - 1);
	// add entry
	uint64_t stack_body_size = 0;
	const uint64_t stack_body_size_offset =
		handler->file_offset + offsetof(unique_stack_header_t, body_size);
	file_pread(handler->file, &stack_body_size, sizeof(stack_body_size), stack_body_size_offset);

	const size_t base_offset = body_offset(handler) + stack_body_size;
	const size_t entry_head_offset = base_offset;
	const size_t body_offset = entry_head_offset + sizeof(entry_head);

	file_pwrite(handler->file, body, size, body_offset);
	file_pwrite(handler->file, &entry_head, sizeof(entry_head), entry_head_offset);

	stack_body_size += sizeof(entry_head) + size;
	file_pwrite(handler->file, &stack_body_size, sizeof(stack_body_size), stack_body_size_offset);
	// return offset of data in file
	return base_offset + sizeof(entry_head);
}

// FNV-1a over size then body. Only used for in-memory indexing during batch
// adds; equality is always confirmed by byte comparison, so collisions are
// harmless. The md5 in the entry head is untouched (file format unchanged).
static uint64_t batch_index_hash(const void *body, uint32_t size)
{
	uint64_t hash = 0xcbf29ce484222325ull;
	const uint8_t *bytes = (const uint8_t *)&size;
	for (size_t i = 0; i < sizeof(size); i++)
		hash = (hash ^ bytes[i]) * 0x100000001b3ull;
	bytes = (const uint8_t *)body;
	for (size_t i = 0; i < size; i++)
		hash = (hash ^ bytes[i]) * 0x100000001b3ull;
	return hash;
}

void unique_stack_add_batch(unique_stack_handler_t *handler, unique_stack_batch_item_t *items,
							size_t count)
{
	RETURN_IF_INVALID(handler);
	if (items == NULL || count == 0)
		return;

	// open-addressing index over the batch items (index + 1, 0 = empty slot),
	// sized to stay at most half full
	size_t table_size = 2;
	while (table_size < 2 * count)
		table_size *= 2;
	const size_t table_mask = table_size - 1;
	size_t *table = memory_heap_allocation(table_size * sizeof(*table));
	memset(table, 0, table_size * sizeof(*table));
	uint64_t *hashes = memory_heap_allocation(count * sizeof(*hashes));
	// representative item for items with identical content within the batch
	size_t *rep = memory_heap_allocation(count * sizeof(*rep));

	for (size_t i = 0; i < count; i++) {
		items[i].out_offset = 0;
		hashes[i] = batch_index_hash(items[i].body, items[i].size);
		rep[i] = i;
		size_t slot = hashes[i] & table_mask;
		while (table[slot] != 0) {
			const size_t other = table[slot] - 1;
			if (hashes[other] == hashes[i] && items[other].size == items[i].size &&
				memcmp(items[other].body, items[i].body, items[i].size) == 0) {
				rep[i] = other;
				break;
			}
			slot = (slot + 1) & table_mask;
		}
		if (rep[i] == i)
			table[slot] = i + 1;
	}

	// read the existing stack body once and match it against the batch
	unique_stack_header_t header = {0};
	file_pread(handler->file, &header, sizeof(header), handler->file_offset);
	uint8_t *existing = NULL;
	if (header.body_size > 0) {
		existing = memory_heap_allocation(header.body_size);
		file_pread(handler->file, existing, header.body_size, body_offset(handler));
		uint64_t offset_in_body = 0;
		while (offset_in_body + sizeof(entry_head_t) <= header.body_size) {
			const entry_head_t *const head = (const entry_head_t *)(existing + offset_in_body);
			const uint64_t entry_body_in_body = offset_in_body + sizeof(entry_head_t);
			if (entry_body_in_body + head->body_size > header.body_size)
				break; // truncated/corrupt tail, treat rest as absent
			const uint8_t *const entry_body = existing + entry_body_in_body;
			const uint64_t entry_hash = batch_index_hash(entry_body, head->body_size);
			size_t slot = entry_hash & table_mask;
			while (table[slot] != 0) {
				const size_t item = table[slot] - 1;
				if (hashes[item] == entry_hash && items[item].size == head->body_size &&
					items[item].out_offset == 0 &&
					memcmp(items[item].body, entry_body, head->body_size) == 0) {
					items[item].out_offset = body_offset(handler) + entry_body_in_body;
					break;
				}
				slot = (slot + 1) & table_mask;
			}
			offset_in_body = entry_body_in_body + head->body_size;
		}
	}

	// append everything that is still unmatched; commit the new body size once
	// at the end so a crash mid-batch leaves the previous consistent state
	uint64_t stack_body_size = header.body_size;
	for (size_t i = 0; i < count; i++) {
		if (rep[i] != i || items[i].out_offset != 0)
			continue;
		entry_head_t entry_head = {
			.md5_hash = hash_function(items[i].body, items[i].size),
			.body_size = items[i].size,
		};
		entry_head.crc = crc8_continue(0, (const uint8_t *)&entry_head, sizeof(entry_head) - 1);
		const uint64_t entry_head_offset = body_offset(handler) + stack_body_size;
		const uint64_t entry_body_offset = entry_head_offset + sizeof(entry_head);
		file_pwrite(handler->file, items[i].body, items[i].size, entry_body_offset);
		file_pwrite(handler->file, &entry_head, sizeof(entry_head), entry_head_offset);
		items[i].out_offset = entry_body_offset;
		stack_body_size += sizeof(entry_head) + items[i].size;
	}
	if (stack_body_size != header.body_size) {
		const uint64_t stack_body_size_offset =
			handler->file_offset + offsetof(unique_stack_header_t, body_size);
		file_pwrite(handler->file, &stack_body_size, sizeof(stack_body_size),
					stack_body_size_offset);
	}

	// resolve items that were duplicates within the batch
	for (size_t i = 0; i < count; i++) {
		if (rep[i] != i)
			items[i].out_offset = items[rep[i]].out_offset;
	}

	if (existing != NULL)
		memory_heap_free(existing);
	memory_heap_free(rep);
	memory_heap_free(hashes);
	memory_heap_free(table);
}