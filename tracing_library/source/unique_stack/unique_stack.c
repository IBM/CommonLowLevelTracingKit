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
	if (handler == NULL)
		return;

	// the in-memory lookup index is owned by the handler; closing releases it
	unique_stack_drop_index(handler);

	handler->valid = 0;
	handler->file = NULL;
	handler->file_offset = 0;
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

// ---------------------------------------------------------------------------
// lookup index
//
// The stack is an append-only log; every lookup used to scan it linearly.
// The index maps a fast content hash to the entry's body offset, kept in
// memory per handler and persisted periodically as tagged entries inside the
// stack itself. Persistence rules (crash consistency by construction):
//  - slabs are ordinary stack entries: body first, head second, body_size
//    commit last - a torn slab is invisible
//  - the last valid slab wins; earlier slabs become dead weight
//  - a missing, torn, or implausible slab degrades to rebuild-by-scan
//  - slab bodies are nibble-encoded (every byte has the high bit set), so
//    pre-1.7.0 decoders scanning for the '{' meta magic never match inside
// ---------------------------------------------------------------------------

typedef struct {
	uint32_t hash;	 // batch_index_hash derived, 0 = empty slot
	uint64_t offset; // absolute file offset of the entry body
} index_slot_t;

struct unique_stack_index_t {
	uint32_t slot_count; // power of two
	uint32_t used;
	uint64_t covered_body_size;	  // stack body prefix reflected in memory
	uint64_t persisted_body_size; // stack body prefix covered by last slab
	index_slot_t *slots;
};

// logical slab layout (before encoding): all fields native byte order
typedef struct __attribute__((packed)) {
	uint32_t magic; // 'X''D''I''1' little pattern, see SLAB_MAGIC
	uint32_t used;
	uint64_t covered_body_size;
} slab_head_t;
#define SLAB_MAGIC 0x31584449u /* "IDX1" */
#define INDEX_PUBLISH_MIN_ENTRIES 16
#define INDEX_PUBLISH_LAG 4096

static uint64_t batch_index_hash(const void *body, uint32_t size);

static uint32_t slot_hash(const void *body, uint32_t size)
{
	const uint64_t h = batch_index_hash(body, size);
	uint32_t folded = (uint32_t)(h ^ (h >> 32));
	return folded ? folded : 1u; // 0 marks empty slots
}

static unique_stack_index_t *index_create(uint32_t slot_count)
{
	unique_stack_index_t *index = memory_heap_allocation(sizeof(*index));
	index->slot_count = slot_count;
	index->used = 0;
	index->covered_body_size = 0;
	index->persisted_body_size = 0;
	index->slots = memory_heap_allocation(slot_count * sizeof(index_slot_t));
	memset(index->slots, 0, slot_count * sizeof(index_slot_t));
	return index;
}

static void index_insert(unique_stack_index_t *index, uint32_t hash, uint64_t offset);

static void index_grow(unique_stack_index_t *index)
{
	const uint32_t old_count = index->slot_count;
	index_slot_t *const old_slots = index->slots;
	index->slot_count = old_count * 2;
	index->slots = memory_heap_allocation(index->slot_count * sizeof(index_slot_t));
	memset(index->slots, 0, index->slot_count * sizeof(index_slot_t));
	index->used = 0;
	for (uint32_t i = 0; i < old_count; i++) {
		if (old_slots[i].hash != 0)
			index_insert(index, old_slots[i].hash, old_slots[i].offset);
	}
	memory_heap_free(old_slots);
}

static void index_insert(unique_stack_index_t *index, uint32_t hash, uint64_t offset)
{
	if ((index->used + 1) * 2 >= index->slot_count)
		index_grow(index);
	uint32_t slot = hash & (index->slot_count - 1);
	while (index->slots[slot].hash != 0) {
		if (index->slots[slot].hash == hash && index->slots[slot].offset == offset)
			return; // already present
		slot = (slot + 1) & (index->slot_count - 1);
	}
	index->slots[slot].hash = hash;
	index->slots[slot].offset = offset;
	index->used++;
}

// verify a candidate by comparing the md5 stored in the entry head in the
// file - the exact dedup semantic the linear scan used
static bool index_candidate_matches(unique_stack_handler_t *handler, uint64_t body_offset,
									__uint128_t md5_hash)
{
	entry_head_t head = {0};
	file_pread(handler->file, &head, sizeof(head), body_offset - sizeof(head));
	return head.md5_hash == md5_hash;
}

static uint64_t index_lookup(unique_stack_handler_t *handler, uint32_t hash, __uint128_t md5_hash)
{
	const unique_stack_index_t *const index = handler->index;
	uint32_t slot = hash & (index->slot_count - 1);
	while (index->slots[slot].hash != 0) {
		if (index->slots[slot].hash == hash &&
			index_candidate_matches(handler, index->slots[slot].offset, md5_hash))
			return index->slots[slot].offset;
		slot = (slot + 1) & (index->slot_count - 1);
	}
	return 0;
}

// byte-comparing variant for callers that avoid md5 on lookups (the batch
// path): verify size via the entry head, then compare the body bytes
static uint64_t index_lookup_bytes(unique_stack_handler_t *handler, uint32_t hash, const void *body,
								   uint32_t size)
{
	const unique_stack_index_t *const index = handler->index;
	uint32_t slot = hash & (index->slot_count - 1);
	while (index->slots[slot].hash != 0) {
		if (index->slots[slot].hash == hash) {
			const uint64_t offset = index->slots[slot].offset;
			entry_head_t head = {0};
			file_pread(handler->file, &head, sizeof(head), offset - sizeof(head));
			if (head.body_size == size) {
				uint8_t *const candidate = memory_heap_allocation(size);
				file_pread(handler->file, candidate, size, offset);
				const bool equal = (memcmp(candidate, body, size) == 0);
				memory_heap_free(candidate);
				if (equal)
					return offset;
			}
		}
		slot = (slot + 1) & (index->slot_count - 1);
	}
	return 0;
}

// nibble encoding: every logical byte becomes two bytes 0x80|nibble, so no
// encoded byte can equal the meta magic '{' (0x7B)
static uint32_t slab_encoded_size(uint32_t logical)
{
	return logical * 2;
}

static void slab_encode(const uint8_t *in, uint32_t in_size, uint8_t *out)
{
	for (uint32_t i = 0; i < in_size; i++) {
		out[2 * (size_t)i] = (uint8_t)(0x80u | (in[i] >> 4));
		out[2 * (size_t)i + 1] = (uint8_t)(0x80u | (in[i] & 0x0Fu));
	}
}

static bool slab_decode(const uint8_t *in, uint32_t in_size, uint8_t *out)
{
	if (in_size % 2 != 0)
		return false;
	for (uint32_t i = 0; i < in_size; i += 2) {
		if ((in[i] & 0x80u) == 0 || (in[i + 1] & 0x80u) == 0)
			return false;
		out[i / 2] = (uint8_t)(((in[i] & 0x0Fu) << 4) | (in[i + 1] & 0x0Fu));
	}
	return true;
}

// walk the stack in [from, upto) and add every meta entry to the in-memory
// index; index slabs are skipped by their kind tag
static void index_scan_range(unique_stack_handler_t *handler, uint64_t from, uint64_t upto)
{
	uint64_t offset_in_body = from;
	while (offset_in_body + sizeof(entry_head_t) <= upto) {
		entry_head_t head = {0};
		file_pread(handler->file, &head, sizeof(head), body_offset(handler) + offset_in_body);
		const uint64_t entry_body = offset_in_body + sizeof(entry_head_t);
		if (entry_body + head.body_size > upto)
			break; // truncated tail: treat as absent
		if (memcmp(head.kind_tag, UNIQUE_STACK_INDEX_TAG, sizeof(head.kind_tag)) != 0) {
			uint8_t *const body = memory_heap_allocation(head.body_size);
			file_pread(handler->file, body, head.body_size, body_offset(handler) + entry_body);
			index_insert(handler->index, slot_hash(body, head.body_size),
						 body_offset(handler) + entry_body);
			memory_heap_free(body);
		}
		offset_in_body = entry_body + head.body_size;
	}
	handler->index->covered_body_size = upto;
}

// locate the last valid persisted slab and load it; returns the stack body
// offset the slab covers, or 0 when rebuilding from scratch
static uint64_t index_load_last_slab(unique_stack_handler_t *handler, uint64_t stack_body_size)
{
	uint64_t offset_in_body = 0;
	uint64_t slab_body = 0;
	uint32_t slab_size = 0;
	while (offset_in_body + sizeof(entry_head_t) <= stack_body_size) {
		entry_head_t head = {0};
		file_pread(handler->file, &head, sizeof(head), body_offset(handler) + offset_in_body);
		const uint64_t entry_body = offset_in_body + sizeof(entry_head_t);
		if (entry_body + head.body_size > stack_body_size)
			break;
		if (memcmp(head.kind_tag, UNIQUE_STACK_INDEX_TAG, sizeof(head.kind_tag)) == 0) {
			slab_body = body_offset(handler) + entry_body;
			slab_size = head.body_size;
		}
		offset_in_body = entry_body + head.body_size;
	}
	if (slab_body == 0 || slab_size < slab_encoded_size(sizeof(slab_head_t)))
		return 0;

	uint8_t *const encoded = memory_heap_allocation(slab_size);
	file_pread(handler->file, encoded, slab_size, slab_body);
	const uint32_t logical_size = slab_size / 2;
	uint8_t *const logical = memory_heap_allocation(logical_size);
	const bool decodable = slab_decode(encoded, slab_size, logical);
	memory_heap_free(encoded);

	uint64_t covered = 0;
	if (decodable) {
		slab_head_t slab = {0};
		memcpy(&slab, logical, sizeof(slab));
		const uint64_t expected = sizeof(slab_head_t) + (uint64_t)slab.used * sizeof(index_slot_t);
		if (slab.magic == SLAB_MAGIC && slab.covered_body_size <= stack_body_size &&
			expected == logical_size) {
			for (uint32_t i = 0; i < slab.used; i++) {
				index_slot_t slot = {0};
				memcpy(&slot, logical + sizeof(slab_head_t) + i * sizeof(slot), sizeof(slot));
				if (slot.hash != 0)
					index_insert(handler->index, slot.hash, slot.offset);
			}
			covered = slab.covered_body_size;
			handler->index->persisted_body_size = covered;
		}
	}
	memory_heap_free(logical);
	return covered;
}

static void index_ensure(unique_stack_handler_t *handler, uint64_t stack_body_size)
{
	if (handler->index == NULL) {
		handler->index = index_create(64);
		const uint64_t covered = index_load_last_slab(handler, stack_body_size);
		index_scan_range(handler, covered, stack_body_size);
	} else if (handler->index->covered_body_size < stack_body_size) {
		// another process appended entries since our last look
		index_scan_range(handler, handler->index->covered_body_size, stack_body_size);
	}
}

static uint64_t append_entry(unique_stack_handler_t *handler, const void *body, uint32_t size,
							 const uint8_t kind_tag[8], __uint128_t md5_hash);

static void index_maybe_publish(unique_stack_handler_t *handler, uint64_t stack_body_size)
{
	unique_stack_index_t *const index = handler->index;
	if (index->used < INDEX_PUBLISH_MIN_ENTRIES)
		return;
	if (index->persisted_body_size != 0 &&
		(stack_body_size - index->persisted_body_size) < INDEX_PUBLISH_LAG)
		return;

	const uint32_t logical_size =
		sizeof(slab_head_t) + (uint32_t)((size_t)index->used * sizeof(index_slot_t));
	uint8_t *const logical = memory_heap_allocation(logical_size);
	const slab_head_t slab = {
		.magic = SLAB_MAGIC,
		.used = index->used,
		// the slab also covers itself once appended
		.covered_body_size =
			stack_body_size + sizeof(entry_head_t) + slab_encoded_size(logical_size),
	};
	memcpy(logical, &slab, sizeof(slab));
	uint32_t written = 0;
	for (uint32_t i = 0; i < index->slot_count && written < index->used; i++) {
		if (index->slots[i].hash == 0)
			continue;
		memcpy(logical + sizeof(slab_head_t) + written * sizeof(index_slot_t), &index->slots[i],
			   sizeof(index_slot_t));
		written++;
	}
	uint8_t *const encoded = memory_heap_allocation(slab_encoded_size(logical_size));
	slab_encode(logical, logical_size, encoded);
	memory_heap_free(logical);

	uint8_t tag[8];
	memcpy(tag, UNIQUE_STACK_INDEX_TAG, sizeof(tag));
	const __uint128_t md5 = hash_function(encoded, slab_encoded_size(logical_size));
	append_entry(handler, encoded, slab_encoded_size(logical_size), tag, md5);
	memory_heap_free(encoded);

	index->persisted_body_size = slab.covered_body_size;
	index->covered_body_size = slab.covered_body_size;
}

void unique_stack_drop_index(unique_stack_handler_t *handler)
{
	if (handler == NULL || handler->index == NULL)
		return;
	memory_heap_free(handler->index->slots);
	memory_heap_free(handler->index);
	handler->index = NULL;
}

static uint64_t append_entry(unique_stack_handler_t *handler, const void *body, uint32_t size,
							 const uint8_t kind_tag[8], __uint128_t md5_hash)
{
	entry_head_t entry_head = {
		.md5_hash = md5_hash,
		.body_size = size,
	};
	if (kind_tag != NULL)
		memcpy(entry_head.kind_tag, kind_tag, sizeof(entry_head.kind_tag));
	entry_head.crc = crc8_continue(0, (const uint8_t *)&entry_head, sizeof(entry_head) - 1);

	uint64_t stack_body_size = 0;
	const uint64_t stack_body_size_offset =
		handler->file_offset + offsetof(unique_stack_header_t, body_size);
	file_pread(handler->file, &stack_body_size, sizeof(stack_body_size), stack_body_size_offset);

	const size_t base_offset = body_offset(handler) + stack_body_size;
	const size_t entry_head_offset = base_offset;
	const size_t entry_body_offset = entry_head_offset + sizeof(entry_head);

	file_pwrite(handler->file, body, size, entry_body_offset);
	file_pwrite(handler->file, &entry_head, sizeof(entry_head), entry_head_offset);

	stack_body_size += sizeof(entry_head) + size;
	file_pwrite(handler->file, &stack_body_size, sizeof(stack_body_size), stack_body_size_offset);

	if (handler->index != NULL)
		handler->index->covered_body_size = stack_body_size;
	return base_offset + sizeof(entry_head);
}

uint64_t unique_stack_add(unique_stack_handler_t *handler, const void *body, uint32_t size)
{
	RETURN_IF_INVALID(handler, 0);

	uint64_t stack_body_size = 0;
	file_pread(handler->file, &stack_body_size, sizeof(stack_body_size),
			   handler->file_offset + offsetof(unique_stack_header_t, body_size));
	index_ensure(handler, stack_body_size);

	const __uint128_t md5_hash = hash_function(body, size);
	const uint32_t hash = slot_hash(body, size);
	const uint64_t existing = index_lookup(handler, hash, md5_hash);
	if (existing > 0)
		return existing;

	const uint64_t entry_offset = append_entry(handler, body, size, NULL, md5_hash);
	index_insert(handler->index, hash, entry_offset);
	index_maybe_publish(handler, stack_body_size + sizeof(entry_head_t) + size);
	return entry_offset;
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

	// match batch items against the stack through the lookup index (loaded
	// from the last persisted slab plus a tail scan, or built by one scan)
	unique_stack_header_t header = {0};
	file_pread(handler->file, &header, sizeof(header), handler->file_offset);
	index_ensure(handler, header.body_size);
	for (size_t i = 0; i < count; i++) {
		if (rep[i] != i)
			continue;
		const uint32_t hash = slot_hash(items[i].body, items[i].size);
		items[i].out_offset = index_lookup_bytes(handler, hash, items[i].body, items[i].size);
	}

	// append everything that is still unmatched. All new entries are staged
	// in one buffer and written with a single call instead of two writes per
	// entry. The new body size is committed once at the end, so a crash
	// mid-batch leaves the previous consistent state.
	uint64_t append_size = 0;
	for (size_t i = 0; i < count; i++) {
		if (rep[i] != i || items[i].out_offset != 0)
			continue;
		append_size += sizeof(entry_head_t) + items[i].size;
	}
	if (append_size > 0) {
		uint8_t *const staging = memory_heap_allocation(append_size);
		uint64_t staged = 0;
		for (size_t i = 0; i < count; i++) {
			if (rep[i] != i || items[i].out_offset != 0)
				continue;
			entry_head_t entry_head = {
				.md5_hash = hash_function(items[i].body, items[i].size),
				.body_size = items[i].size,
			};
			entry_head.crc = crc8_continue(0, (const uint8_t *)&entry_head, sizeof(entry_head) - 1);
			memcpy(staging + staged, &entry_head, sizeof(entry_head));
			memcpy(staging + staged + sizeof(entry_head), items[i].body, items[i].size);
			items[i].out_offset =
				body_offset(handler) + header.body_size + staged + sizeof(entry_head);
			staged += sizeof(entry_head) + items[i].size;
		}
		file_pwrite(handler->file, staging, append_size, body_offset(handler) + header.body_size);
		memory_heap_free(staging);

		const uint64_t stack_body_size = header.body_size + append_size;
		const uint64_t stack_body_size_offset =
			handler->file_offset + offsetof(unique_stack_header_t, body_size);
		file_pwrite(handler->file, &stack_body_size, sizeof(stack_body_size),
					stack_body_size_offset);

		// register the appended entries and refresh the persisted slab
		for (size_t i = 0; i < count; i++) {
			if (rep[i] != i || items[i].out_offset == 0)
				continue;
			index_insert(handler->index, slot_hash(items[i].body, items[i].size),
						 items[i].out_offset);
		}
		handler->index->covered_body_size = stack_body_size;
		index_maybe_publish(handler, stack_body_size);
	}

	// resolve items that were duplicates within the batch
	for (size_t i = 0; i < count; i++) {
		if (rep[i] != i)
			items[i].out_offset = items[rep[i]].out_offset;
	}

	memory_heap_free(rep);
	memory_heap_free(hashes);
	memory_heap_free(table);
}