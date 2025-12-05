// Copyright (c) 2024, International Business Machines
// SPDX-License-Identifier: BSD-2-Clause-Patent

#pragma once
#ifdef __cplusplus
extern "C" {
#endif

#include "abstraction/sync.h"

#if defined(__KERNEL__)
#include <linux/types.h>
#else
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#endif

/**
 * @brief ringbuffer head, placed at the beginning of ringbuffer.
 */
struct ringbuffer_head_t;
typedef struct ringbuffer_head_t ringbuffer_head_t;

struct __attribute__((packed, aligned(8))) ringbuffer_head_t {
	uint64_t version;
	sync_mutex_t mutex;
	uint64_t body_size;	 // size of body which is used for holding entries
	uint64_t wrapped;	 // will never overflow
	uint64_t dropped;	 // will never overflow
	uint64_t entries;	 // will never overflow
	uint64_t next_free;	 // next free for write to ( 0 <= next_free < body_size)
	uint64_t last_valid; // next last valid for read from ( 0 <= last_valid < body_size)
	uint8_t _reserved_for_future_use[40];
	uint8_t body[];
};

static const uint8_t ringbuffer_entry_magic = '~';

typedef uint16_t ringbuffer_entry_body_size_t;
typedef struct __attribute__((packed)) {
	uint8_t magic;
	ringbuffer_entry_body_size_t body_size;
	uint8_t crc;
	uint8_t body[];
	/*
	after body also crc8 (not part of body_size)
	*/
} ringbuffer_entry_head_t;

/**
 * @brief checks if ringbuffer is valid and usable
 *
 * @param ringbuffer to be checked
 * @return true if valid else false
 */
bool ringbuffer_valid(const ringbuffer_head_t *ringbuffer);

/**
 * @brief initialize ringbuffer in given space and returns pointer to this ringbuffer
 *
 * @param space pointer to not yet initialized space for ringbuffer.
 * Needs to be valid while ringbuffer is used
 * @param space_size size of space for ringbuffer
 * @return ringbuffer_head_t* pointer to initialized ringbuffer.
 * If failed, ringbuffer_valid returns false for this pointer.
 */
ringbuffer_head_t *ringbuffer_init(void *space, size_t space_size);

/**
 * @brief open an already initialized ringbuffer
 *
 * @param space pointer to a space with a already initialized ringbuffer
 * @return ringbuffer_head_t* pointer to open ringbuffer.
 * If failed, ringbuffer_valid returns false for this pointer.
 */
ringbuffer_head_t *ringbuffer_open(void *space);

/**
 * @brief add something to ringbuffer
 *
 * @param destination ringbuffer
 * @param source pointer to data to be written to buffer
 * @param size size of data to be written
 * @return size_t number of written bytes. 0 if failed else `size`.
 */
size_t ringbuffer_in(ringbuffer_head_t *destination, const void *source, size_t size);

/**
 * @brief move out oldest valid entry from ringbuffer
 *
 * @param destination output buffer to which oldest entry is moved
 * @param max_size size of output buffer
 * @param source ringbuffer
 * @return size_t number of bytes written to destination
 * If destination is to small for entry, returns required size and does not removes entry from
 * buffer. And 0 if failed/no entry
 */
size_t ringbuffer_out(void *destination, size_t max_size, ringbuffer_head_t *source);

/**
 * @brief
 *
 * @param rb ringbuffer
 * @return uint64_t
 */
uint64_t ringbuffer_capacity(ringbuffer_head_t *rb);

/**
 * @brief
 *
 * @param rb ringbuffer
 * @return uint64_t
 */
uint64_t ringbuffer_occupied(ringbuffer_head_t *rb);

/**
 * @brief
 *
 * @param rb ringbuffer
 * @return uint64_t
 */
uint64_t ringbuffer_available(ringbuffer_head_t *rb);

/**
 * @brief Clear all entries from ringbuffer
 *
 * Sets last_valid = next_free to make buffer appear empty.
 * Updates dropped counter to reflect all entries as dropped.
 * Does not modify next_free, wrapped, or entries counters.
 *
 * @param rb ringbuffer to clear
 */
void ringbuffer_clear(ringbuffer_head_t *rb);

#ifdef __cplusplus
}
#endif