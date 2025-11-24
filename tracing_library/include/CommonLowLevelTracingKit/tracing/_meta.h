// Copyright (c) 2024, International Business Machines
// SPDX-License-Identifier: BSD-2-Clause-Patent

#ifndef _CLLTK_META_H_
#define _CLLTK_META_H_
// IWYU pragma: private, include "CommonLowLevelTracingKit/tracing/_tracing.h"

#include "CommonLowLevelTracingKit/tracing/_arguments.h"
#include "CommonLowLevelTracingKit/tracing/_macros.h"

#if defined(__KERNEL__)
#include <linux/limits.h>
#include <linux/types.h>
#define UINT64_MAX U64_MAX
#define UINT32_MAX U32_MAX
#else
#include <stdint.h>
#endif

#ifdef CLLTK_FOR_CPP
extern "C" {
#endif

typedef enum __attribute__((packed)) {
	_clltk_meta_enty_type_printf = 1,
	_clltk_meta_enty_type_dump = 2,
} _clltk_meta_enty_type;
_CLLTK_STATIC_ASSERT(sizeof(_clltk_meta_enty_type) == 1, "should be of size 1");

#define _CLLTK_CREATE_META_ENTRY_ARGS(_VAR_, _ATTRIBUTE_, _STR_, ...)                            \
                                                                                                 \
	_CLLTK_STATIC_ASSERT(__LINE__ <= UINT32_MAX, "line fits not in uint32");                     \
	static const struct __attribute__((packed, aligned(1))) {                                    \
		const char magic;                                                                        \
		const uint32_t size;                                                                     \
		const _clltk_meta_enty_type type;                                                        \
		const uint32_t line;                                                                     \
		const uint8_t argument_count;                                                            \
		const char argument_type_array[_CLLTK_NARGS(__VA_ARGS__) + 1];                           \
		const char file[sizeof(__FILE__)];                                                       \
		const char str[sizeof(_STR_)];                                                           \
	} _VAR_ _ATTRIBUTE_ = {/*.magic = */ '{',                                                    \
						   /*.size = */ sizeof(_VAR_),                                           \
						   /*.type = */ _clltk_meta_enty_type_printf,                            \
						   /*.line = */ __LINE__,                                                \
						   /*.argument_count = */ _CLLTK_NARGS(__VA_ARGS__),                     \
						   /*.argument_type_array = */ {_CLLTK_ARG_TYPES_TO_TYPES(__VA_ARGS__)}, \
						   /*.file = */ {__FILE__},                                              \
						   /*.str = */ {_STR_}}

#define _CLLTK_CREATE_META_ENTRY_DUMP(_VAR_, _ATTRIBUTE_, _MSG_)               \
                                                                               \
	_CLLTK_STATIC_ASSERT(__LINE__ <= UINT32_MAX, "line fits not in uint32");   \
	static const struct __attribute__((packed)) {                              \
		const char magic;                                                      \
		const uint32_t size;                                                   \
		const _clltk_meta_enty_type type;                                      \
		const uint32_t line;                                                   \
		const uint8_t argument_count;                                          \
		const char argument_type_array[1 + 1];                                 \
		const char file[sizeof(__FILE__)];                                     \
		const char str[sizeof(_MSG_)];                                         \
	} _VAR_ _ATTRIBUTE_ = {/*.magic = */ '{',                                  \
						   /*.size = */ sizeof(_VAR_),                         \
						   /*.type = */ _clltk_meta_enty_type_dump,            \
						   /*.line = */ __LINE__,                              \
						   /*.argument_count = */ 1,                           \
						   /*.argument_type_array = */ {_clltk_argument_dump}, \
						   /*.file = */ {__FILE__},                            \
						   /*.str = */ {_MSG_}}

#ifdef CLLTK_FOR_CPP
}
#endif

#endif
