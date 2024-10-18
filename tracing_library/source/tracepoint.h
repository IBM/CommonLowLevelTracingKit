// Copyright (c) 2024, International Business Machines
// SPDX-License-Identifier: BSD-2-Clause-Patent

#pragma once

#if defined(__KERNEL__)
#include <linux/types.h>
#else
#include <stdint.h>
#endif

typedef struct __attribute__((packed)) {
	uint64_t in_file_offset : 48;
	uint32_t pid;
	uint32_t tid;
	uint64_t timestamp_ns;
	uint8_t body[];
} traceentry_head_t;
