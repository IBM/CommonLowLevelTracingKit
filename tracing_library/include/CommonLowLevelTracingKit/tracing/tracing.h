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
fmt-style static tracepoint (C++20 only)
- format strings use {} placeholders, validated against the argument types at
  compile time via std::format_string
- same speed and mechanics as CLLTK_TRACEPOINT, only the format syntax differs
- a char* argument is always recorded as a string ({} has no printf-style
  %p/%s ambiguity); cast to void* to record the pointer value

example:
	CLLTK_TRACEPOINT_FMT(some_tracebuffer, "loaded {} in {}ms", name, duration);
*/
#define CLLTK_TRACEPOINT_FMT(_BUFFER_, _FORMAT_, ...) \
	_CLLTK_STATIC_TRACEPOINT_FMT(_BUFFER_, _FORMAT_ __VA_OPT__(, ) __VA_ARGS__)

/*
span tracking with a carryable id
- CLLTK_SPAN_BEGIN records a span-begin event and evaluates to the new span id
- the id is a plain value: pass it as function argument, across threads, or
  embed it in APIs; a sub-span passes the surrounding span's id as parent
- CLLTK_SPAN_END records the matching end event
- the decoder correlates begin/end by id across all buffers of a decode set
  and shows spans that were still open when the process ended

example:
	clltk_span_id_t span = CLLTK_SPAN_BEGIN(some_tracebuffer,
		CLLTK_SPAN_NO_PARENT, "request handling");
	clltk_span_id_t sub = CLLTK_SPAN_BEGIN(some_tracebuffer, span, "parsing");
	...
	CLLTK_SPAN_END(some_tracebuffer, sub);
	CLLTK_SPAN_END(some_tracebuffer, span);
*/
#define CLLTK_SPAN_BEGIN(_BUFFER_, _PARENT_, _NAME_) \
	_CLLTK_STATIC_SPAN_BEGIN(_BUFFER_, _PARENT_, _NAME_)

#define CLLTK_SPAN_END(_BUFFER_, _ID_) _CLLTK_STATIC_SPAN_END(_BUFFER_, _ID_)

#define CLLTK_SPAN_NO_PARENT ((clltk_span_id_t)0)

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

/* returns a new process-unique span id; never returns 0 (= no parent) */
clltk_span_id_t clltk_next_span_id(void) __attribute__((used, visibility("default")));

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
