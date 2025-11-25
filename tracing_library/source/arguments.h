// Copyright (c) 2024, International Business Machines
// SPDX-License-Identifier: BSD-2-Clause-Patent

#pragma once

#if defined(__KERNEL__)
#include <linux/types.h>
#include <linux/version.h>
#if LINUX_VERSION_CODE > (KERNEL_VERSION(5, 14, 0))
#include <linux/stdarg.h>
#else
#include <stdarg.h>
#endif

#else
#include <stdarg.h>
#include <stdint.h>
#endif

#include "CommonLowLevelTracingKit/tracing/tracing.h"

#ifdef __cplusplus
extern "C" {
#endif

uint32_t get_argument_sizes(const char *const format, uint32_t sizes_out[],
							_clltk_argument_types_t *types, va_list args);
void get_arguments(void *buffer, uint32_t sizes[], const _clltk_argument_types_t *types,
				   va_list args);

void first_time_check(const char *const format, _clltk_argument_types_t *types);

#ifdef __cplusplus
}
#endif
