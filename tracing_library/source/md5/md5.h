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
#include <stdint.h>
#endif

typedef struct {
	uint64_t size;		// Size of input in bytes
	uint32_t buffer[4]; // Current accumulation of hash
	uint8_t input[64];	// Input to be used in the next step
	uint8_t digest[16]; // Result of algorithm
} MD5Context;

void md5Init(MD5Context *ctx);
void md5Update(MD5Context *ctx, const uint8_t *input, size_t input_len);
void md5Finalize(MD5Context *ctx);
void md5Step(uint32_t *buffer, const uint32_t *input);

uint32_t rotateLeft(uint32_t x, uint32_t n);

#ifdef __cplusplus
}
#endif