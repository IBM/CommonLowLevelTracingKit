// Copyright (c) 2024, International Business Machines
// SPDX-License-Identifier: BSD-2-Clause-Patent

#pragma once
#ifdef __cplusplus
extern "C" {
#endif

#include "abstraction/file.h"
#include "abstraction/sync.h"

#if defined(__KERNEL__)
#include <linux/types.h>
#else
#include <stdbool.h>
#include <stdint.h>
#endif

struct unique_stack_header_t;
typedef struct unique_stack_header_t unique_stack_header_t;
struct __attribute__((packed, aligned(8))) unique_stack_header_t {
	uint64_t version;
	sync_mutex_t mutex;
	uint8_t _reserved_for_future_use[40];
	uint64_t body_size;
	// body is not mmapped, therefor not defined here
};

struct unique_stack_handler_t;
typedef struct unique_stack_handler_t unique_stack_handler_t;
struct unique_stack_handler_t {
	bool valid;
	file_t *file;
	uint64_t file_offset;
};

// placed in file at the begin of each stack entry
struct entry_head_t;
typedef struct entry_head_t entry_head_t;
struct __attribute__((packed, aligned(1))) entry_head_t {
	__uint128_t md5_hash;
	uint8_t _reserved_for_future_use[8];
	uint32_t body_size;
	uint8_t crc;
	uint8_t body[];
};

/**
 * init stack in file at given offset
 */
unique_stack_handler_t unique_stack_init(file_t *fh, uint64_t file_offset);

/**
 * open an previously initialized stack from file at given offset
 */
unique_stack_handler_t unique_stack_open(file_t *fh, uint64_t file_offset);

/**
 * close stack handler and drop all allocated resources
 */
void unique_stack_close(unique_stack_handler_t *handler);

/**
 * check if stack is valid and therefor usable
 */
bool unique_stack_valid(const unique_stack_handler_t *handler);

// add data to stack and return offset in file for data
uint64_t unique_stack_add(unique_stack_handler_t *handler, const void *body, uint32_t size);

#ifdef __cplusplus
}
#endif