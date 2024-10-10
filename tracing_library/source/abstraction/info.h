// Copyright (c) 2024, International Business Machines
// SPDX-License-Identifier: BSD-2-Clause-Patent

#pragma once
#ifdef __cplusplus
extern "C" {
#endif

#if defined(__KERNEL__)
#include <linux/types.h>
#else
#include <stdint.h>
#endif

uint64_t info_get_timestamp_ns(void);
uint32_t info_get_thread_id(void);
uint32_t info_get_process_id(void);

#ifdef __cplusplus
}
#endif
