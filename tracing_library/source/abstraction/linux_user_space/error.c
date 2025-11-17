// Copyright (c) 2024, International Business Machines
// SPDX-License-Identifier: BSD-2-Clause-Patent

#include "abstraction/error.h"

#if defined(__KERNEL__)
#include <linux/kernel.h>
#include <linux/stdarg.h>
#else
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#endif

__attribute__((noreturn)) void default_unrecoverbale_error_callback(const char *const message)
{
	fprintf(stderr, "%s", message);
	exit(EXIT_FAILURE);
}

void clltk_unrecoverbale_error_callback(const char *const)
	__attribute__((weak, noreturn, alias("default_unrecoverbale_error_callback")));

void unrecoverable_error(const char *file, size_t line, const char *func, const char *format, ...)
{
	char clltk_message[1024] = "";
	char clltk_format[1024] = "";
#define CML_RED "\033[1;31m"
#define CML_RESET "\033[0m"
	snprintf(clltk_format, sizeof(clltk_format),
			 CML_RED "clltk unrecoverable: %s (%s in %s:%lu)" CML_RESET "\n", format, func, file,
			 line);
#undef CML_RED
#undef CML_RESET
	va_list args;
	va_start(args, format);
	vsnprintf(clltk_message, sizeof(clltk_message), clltk_format, args);
	va_end(args);
	clltk_unrecoverbale_error_callback(clltk_message);
}

void recoverable_error(const char *file, size_t line, const char *func, const char *format, ...)
{
	char clltk_format[1024];
#define CML_RED "\033[1;31m"
#define CML_RESET "\033[0m"
	snprintf(clltk_format, sizeof(clltk_format),
			 CML_RED "clltk recoverable: %s (%s in %s:%lu)" CML_RESET "\n", format, func, file,
			 line);
#undef CML_RED
#undef CML_RESET
	va_list args;
	va_start(args, format);
	vfprintf(stderr, clltk_format, args);
	va_end(args);
}