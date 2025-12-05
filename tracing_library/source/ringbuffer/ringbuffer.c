// Copyright (c) 2024, International Business Machines
// SPDX-License-Identifier: BSD-2-Clause-Patent

#include "ringbuffer.h"
#include "abstraction/memory.h"
#include "crc8/crc8.h"

#if defined(__KERNEL__)
#include <linux/limits.h>
#include <linux/string.h>
#include <linux/types.h>

#define UINT64_MAX U64_MAX
#else
#include <stdint.h>
#include <string.h>
#endif

// add with well behaved overflow
static void add_with_max(uint64_t *const base, uint64_t value)
{
	const uint64_t remaining_space = UINT64_MAX - *base;
	if (value > remaining_space) {
		*base = UINT64_MAX;
	} else {
		(*base) += value;
	}
}

bool ringbuffer_valid(const ringbuffer_head_t *ringbuffer)
{
	return (ringbuffer != NULL) && (ringbuffer->body_size > 0);
}

#define RETURN_IF_INVALID(_RB_, ...) \
	if (!ringbuffer_valid(_RB_))     \
		return __VA_ARGS__;

ringbuffer_head_t *ringbuffer_init(void *space, size_t space_size)
{
	const bool space_invalied = (space == NULL);
	const bool to_small = (space_size <= (sizeof(ringbuffer_head_t) + 1));
	if (space_invalied || to_small)
		return NULL;

	ringbuffer_head_t *rb = (void *)space;
	memset(rb, 0, sizeof(ringbuffer_head_t));

	rb->version = 1;
	rb->body_size = space_size - sizeof(*rb);
	rb->wrapped = 0;
	rb->dropped = 0;
	rb->entries = 0;
	rb->next_free = 0;
	rb->last_valid = 0;
	rb->body[rb->body_size - 1] = 0; // test if we have write access to full body

	return rb;
}

ringbuffer_head_t *ringbuffer_open(void *space)
{
	ringbuffer_head_t *rb = (void *)space;
	if (ringbuffer_valid(rb))
		return rb;
	return NULL;
}

uint64_t ringbuffer_capacity(ringbuffer_head_t *rb)
{
	RETURN_IF_INVALID(rb, 0);
	return rb->body_size - 1;
}

uint64_t ringbuffer_occupied(ringbuffer_head_t *rb)
{
	RETURN_IF_INVALID(rb, 0);
	if (rb->last_valid == rb->next_free)
		// buffer is empty
		return 0;
	else if (rb->next_free > rb->last_valid)
		// buffer currently with out wrapping
		return (rb->next_free - rb->last_valid);
	else
		// buffer currently wrapped to calculate for this
		return (rb->body_size - rb->last_valid) + rb->next_free;
}
uint64_t ringbuffer_available(ringbuffer_head_t *rb)
{
	return ringbuffer_capacity(rb) - ringbuffer_occupied(rb);
}

/*
 * move `next_free`, constrain value to `body_size`.
 * Increment `wrapped` if `next_free` wrapped
 * But does not move `last_valid`.
 */
static void move_next_free(ringbuffer_head_t *rb, size_t n)
{
	rb->next_free += n;
	if (rb->next_free >= rb->body_size) {
		add_with_max(&rb->wrapped, 1);
	}
	rb->next_free %= rb->body_size;
}
/*
 * copy data in to ringbuffer, without updating next_free or last_valid.
 */
static void copy_in(ringbuffer_head_t *destination, const void *source, size_t size)
{
	/*
	copy in two blocks.
	first block is form `next_free` till `body_size`or `next_free`+`size`
	second block is from `&body[0]` with size of remaining data
	*/
	const uint64_t free_space_until_wrapped = destination->body_size - destination->next_free;
	const uint64_t first_block_size =
		(free_space_until_wrapped < size) ? free_space_until_wrapped : size;
	void *const first_block_destination = &destination->body[destination->next_free];
	const void *const first_block_source = source;
	memcpy_and_flush(first_block_destination, first_block_source, first_block_size);
	const uint64_t second_block_size = size - first_block_size;
	void *const second_block_destination = &destination->body[0];
	const void *const second_block_source = (void *)((uint64_t)source + first_block_size);
	memcpy_and_flush(second_block_destination, second_block_source, second_block_size);
}

/*
 * move `last_valid`, constrain value to `body_size`.
 * But does not move `next_free`.
 */
static void move_last_valid(ringbuffer_head_t *rb, size_t n)
{
	rb->last_valid += n;
	rb->last_valid %= rb->body_size;
}

/*
 * copy data out from ringbuffer,  without updating next_free or last_valid.
 */
static void copy_out(void *destination, size_t size, ringbuffer_head_t *source)
{
	/*
	copy in two blocks.
	first block is form `last_valid` till `body_size`or `last_valid`+`size`
	second block is from `&body[0]` with size of remaining data
	*/

	const uint64_t valid_space_until_wrapped = source->body_size - source->last_valid;
	const uint64_t first_block_size =
		(valid_space_until_wrapped < size) ? valid_space_until_wrapped : size;
	void *const first_block_destination = destination;
	void *const first_block_source = &source->body[source->last_valid];
	memcpy(first_block_destination, first_block_source, first_block_size);
	const uint64_t second_block_size = size - first_block_size;
	void *const second_block_destination = (void *)((uint64_t)destination + first_block_size);
	void *const second_block_source = &source->body[0];
	memcpy(second_block_destination, second_block_source, second_block_size);
}

static uint8_t inplace_crc(const ringbuffer_head_t *const rb, size_t offset, size_t size)
{

	uint8_t crc = 0;
	const uint64_t free_space_until_wrapped = rb->body_size - offset;

	const uint64_t first_block_size =
		(free_space_until_wrapped < size) ? free_space_until_wrapped : size;
	const void *const first_block_location = &rb->body[offset];
	crc = crc8_continue(crc, first_block_location, first_block_size);

	const uint64_t second_block_size = size - first_block_size;
	const void *const second_block_location = &rb->body[0];
	crc = crc8_continue(crc, second_block_location, second_block_size);
	return crc;
}

/*
 * drop oldest entry.
 * There for accessing oldest entry body size and move
 * `last_valid` by entry overhead and entry body size.
 */
static void drop_oldest_entry(ringbuffer_head_t *rb)
{
	uint64_t occupied = ringbuffer_occupied(rb);
	bool found_invalid_data = false;
	while (occupied) {
		if (occupied <= sizeof(ringbuffer_entry_head_t)) {
			move_last_valid(rb, occupied);
			return;
		} else if (rb->body[rb->last_valid] != ringbuffer_entry_magic) {
			move_last_valid(rb, 1);
			found_invalid_data = true;
			occupied--;
			continue;
		} else if (0 != inplace_crc(rb, rb->last_valid, sizeof(ringbuffer_entry_head_t))) {
			found_invalid_data = true;
			move_last_valid(rb, 1);
			continue;
		} else if (found_invalid_data) { // we already dropped some stuff and next would be valid
										 // header
			return;
		} else // now magic valid and enough occupied
		{
			// drop entry head
			ringbuffer_entry_head_t entry_head = {0};
			copy_out(&entry_head, sizeof(entry_head), rb);
			move_last_valid(rb, sizeof(entry_head));
			occupied -= sizeof(entry_head);
			add_with_max(&rb->dropped, 1);

			// drop body if valid
			if (occupied < (entry_head.body_size + 1ul)) {
				// not enough data in ringbuffer for this body, so body is invalid -> not dropped
				return;
			} else if (0 != inplace_crc(rb, rb->last_valid, entry_head.body_size + 1ul)) {
				// crc over body is invalid, so body is invalid -> not dropped
				return;
			} else {
				// body is valid and could be dropped
				move_last_valid(rb, entry_head.body_size + 1ul);
				return;
			}
		}
	}
	return;
}

size_t ringbuffer_in(ringbuffer_head_t *destination, const void *source, size_t size)
{
	RETURN_IF_INVALID(destination, 0);
	if ((source == NULL) || (size == 0) ||
		(size >= (destination->body_size -
				  sizeof(ringbuffer_entry_head_t))) || // check if bigger than ringbuffer
		(size >=
		 (((size_t)1)
		  << (8 * sizeof(ringbuffer_entry_body_size_t))))) // check if fit in body size data type
		return 0;

	ringbuffer_entry_head_t entry_header = {
		.magic = ringbuffer_entry_magic, .body_size = (ringbuffer_entry_body_size_t)size, .crc = 0};
	entry_header.crc = crc8_continue(0, (void *)&entry_header, sizeof(entry_header) - 1);
	const uint8_t body_crc = crc8_continue(0, source, size);

	// make space for next entry
	while ((sizeof(entry_header) + size + sizeof(body_crc)) > ringbuffer_available(destination))
		drop_oldest_entry(destination);

	// add entry
	copy_in(destination, &entry_header, sizeof(entry_header));
	move_next_free(destination, sizeof(entry_header));

	copy_in(destination, source, size);
	move_next_free(destination, size);

	copy_in(destination, &body_crc, sizeof(body_crc));
	move_next_free(destination, sizeof(body_crc));

	add_with_max(&destination->entries, 1);

	return size;
}

size_t ringbuffer_out(void *destination, size_t max_size, ringbuffer_head_t *source)
{
	RETURN_IF_INVALID(source, 0);
	if ((destination == NULL) || (max_size == 0))
		return 0;

	size_t occupied = ringbuffer_occupied(source);
	while (occupied) {
		if (occupied <= sizeof(ringbuffer_entry_head_t)) { // not enough in ringbuffer
			move_last_valid(source, occupied);
			return 0;
		} else if (source->body[source->last_valid] != ringbuffer_entry_magic) { // wrong magic
			move_last_valid(source, 1);
			occupied--;
			continue;
		} else if (0 != inplace_crc(source, source->last_valid,
									sizeof(ringbuffer_entry_head_t))) { // entry_header invalid
			move_last_valid(source, 1);
			continue;
		} else { // now entry_header valid
			ringbuffer_entry_head_t entry_head = {0};
			copy_out(&entry_head, sizeof(entry_head), source);
			move_last_valid(source, sizeof(entry_head));
			occupied -= sizeof(entry_head);

			// copy body out if valid
			if (occupied < (entry_head.body_size + 1ul)) {
				// not enough data in ringbuffer for this body, so body is invalid -> not copy
				continue;
			} else if (0 != inplace_crc(source, source->last_valid, entry_head.body_size + 1)) {
				// crc over body is invalid, so body is invalid -> not copy
				continue;
			} else {
				// body is valid and could be copied
				copy_out(destination, entry_head.body_size, source);
				move_last_valid(source, entry_head.body_size + 1);
				return entry_head.body_size;
			}
		}
	}
	return 0;
}

void ringbuffer_clear(ringbuffer_head_t *rb)
{
	RETURN_IF_INVALID(rb, );
	rb->dropped = rb->entries;		// All entries now considered dropped
	rb->last_valid = rb->next_free; // Empty the buffer
}
