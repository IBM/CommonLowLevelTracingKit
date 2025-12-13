// Copyright (c) 2024, International Business Machines
// SPDX-License-Identifier: BSD-2-Clause-Patent

#ifndef _CLLTK_TRACING_H_
#define _CLLTK_TRACING_H_

// Choose implementation based on environment
#if defined(__KERNEL__)
#include "CommonLowLevelTracingKit/tracing/_kernel_tracing.h"
#else
#include "CommonLowLevelTracingKit/tracing/_user_tracing.h"
#endif

#define CLLTK_MAX_NAME_SIZE 255

#define CLLTK_MAX_FILENAME_SIZE 4096

/*
value for tracebuffer identifier as macro define

example:
	#define MyFristTracebuffer CLLTK_TRACEBUFFER_MACRO_VALUE(MyFristTracebuffer);
*/
#define CLLTK_TRACEBUFFER_MACRO_VALUE(_NAME_) _CLLTK_TRACEBUFFER_MACRO_VALUE(_NAME_)

/*
create tracebuffer

example:
	CLLTK_TRACEBUFFER(some_tracebuffer, 1024);
*/
#define CLLTK_TRACEBUFFER(_NAME_, _SIZE_) _CLLTK_STATIC_TRACEBUFFER(_NAME_, _SIZE_)

/*
static tracepoint
- assigned to a tracebuffer at compile time
- with maximal speed

example:
	CLLTK_TRACEPOINT(some_tracebuffer,
		"this is the format %s %u", "string arg", 42);
*/
#define CLLTK_TRACEPOINT(_BUFFER_, _FORMAT_, ...) \
	_CLLTK_STATIC_TRACEPOINT(_BUFFER_, _FORMAT_ __VA_OPT__(, __VA_ARGS__))

/*
static tracepoint
- assigned to a tracebuffer at compile time
- with maximal speed

example:
	char data[10] = ...;
	...
	CLLTK_TRACEPOINT_DUMP(some_tracebuffer, "dump message", data, sizeof(data));
*/
#define CLLTK_TRACEPOINT_DUMP(_BUFFER_, _MSG_, _ADDRESS_, _SIZE_) \
	_CLLTK_STATIC_TRACEPOINT_DUMP(_BUFFER_, _MSG_, _ADDRESS_, _SIZE_)

/*
dynamic tracepoint
- assigned to a tracebuffer at runtime time
- slower than static tracepoint

example:
	CLLTK_DYN_TRACEPOINT("some_tracebuffer",
		"this is the format %s %u", "string arg", 42);
*/
#define CLLTK_DYN_TRACEPOINT(_BUFFER_, _FORMAT_, ...)                      \
	clltk_dynamic_tracepoint_execution(_BUFFER_, __FILE__, __LINE__, 0, 0, \
									   _FORMAT_ __VA_OPT__(, __VA_ARGS__))

#ifdef CLLTK_FOR_CPP
extern "C" {
#endif

void clltk_dynamic_tracebuffer_creation(const char *buffer_name, size_t size)
	__attribute__((used, visibility("default")));

void clltk_dynamic_tracepoint_execution(const char *buffer_name, const char *file, size_t line,
										const uint32_t pid, const uint32_t tid, const char *format,
										...)
	__attribute__((format(printf, 6, 7), used, visibility("default")));

void clltk_dynamic_tracebuffer_clear(const char *buffer_name)
	__attribute__((used, visibility("default")));

void clltk_set_tracing_path(const char *path) __attribute__((used, visibility("default")));

void clltk_unrecoverbale_error_callback(const char *const) __attribute__((weak, noreturn));

#ifdef CLLTK_FOR_CPP
}
#endif

#endif
