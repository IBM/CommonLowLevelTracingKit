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
#endif

// stores error message and than terminates program ( or whole system )
void unrecoverable_error(const char *file, size_t line, const char *func, const char *format, ...)
	__attribute__((format(printf, 4, 5), noreturn));

// stores error message and than terminates program ( or whole system )
void recoverable_error(const char *file, size_t line, const char *func, const char *format, ...)
	__attribute__((format(printf, 4, 5)));

#define ERROR_AND_EXIT(FORMAT, ...) \
	unrecoverable_error(__FILE__, __LINE__, __PRETTY_FUNCTION__, FORMAT __VA_OPT__(, __VA_ARGS__))
#define ERROR_LOG(FORMAT, ...) \
	recoverable_error(__FILE__, __LINE__, __PRETTY_FUNCTION__, FORMAT __VA_OPT__(, __VA_ARGS__))

#ifdef __cplusplus
}
#endif
