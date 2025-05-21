// Copyright (c) 2024, International Business Machines
// SPDX-License-Identifier: BSD-2-Clause-Patent

#ifndef _CLLTK__TRACING_H_
#define _CLLTK__TRACING_H_
// IWYU pragma: private, include "CommonLowLevelTracingKit/tracing.h"

#ifndef _CLLTK_TRACING_H_
#error "CommonLowLevelTracingKit: always use "CommonLowLevelTracingKit/tracing.h" and not this file"
#endif

#if defined(__KERNEL__)
#error "use this header only in user space"
#endif

#include "CommonLowLevelTracingKit/_arguments.h"
#include "CommonLowLevelTracingKit/_internal.h"
#include "CommonLowLevelTracingKit/_macros.h"
#include "CommonLowLevelTracingKit/_meta.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#define _CLLTK_TRACEBUFFER_MACRO_VALUE(_NAME_) _NAME_

#if !defined(_CLLTK_INTERNAL)
_CLLTK_EXTERN_C_BEGIN

__attribute__((constructor(101), used)) static void _clltk_constructor(void)
{
	static const char _empty[]
		__attribute__((used, section("_clltk_tracebuffer_handler_ptr"))) = {};
	extern _clltk_tracebuffer_handler_t *const __start__clltk_tracebuffer_handler_ptr;
	extern _clltk_tracebuffer_handler_t *const __stop__clltk_tracebuffer_handler_ptr;
	for (_clltk_tracebuffer_handler_t *const *handler_ptr = &__start__clltk_tracebuffer_handler_ptr;
		 handler_ptr < &__stop__clltk_tracebuffer_handler_ptr; handler_ptr++) {
		_clltk_tracebuffer_handler_t *const handler = *handler_ptr;
		const uint32_t meta_size =
			(uint32_t)((uint64_t)handler->meta.stop - (uint64_t)handler->meta.start);
		if (meta_size <= 0) {
			continue;
		}
		_clltk_tracebuffer_init(handler);
		if (handler->runtime.file_offset == _clltk_file_offset_unset) {
			handler->runtime.file_offset =
				_clltk_tracebuffer_add_to_stack(handler, handler->meta.start, meta_size);
		}
	}
}

__attribute__((destructor(101), used)) static void _clltk_destructor(void)
{
	extern _clltk_tracebuffer_handler_t *const __start__clltk_tracebuffer_handler_ptr;
	extern _clltk_tracebuffer_handler_t *const __stop__clltk_tracebuffer_handler_ptr;
	for (_clltk_tracebuffer_handler_t *const *handler_ptr = &__start__clltk_tracebuffer_handler_ptr;
		 handler_ptr < &__stop__clltk_tracebuffer_handler_ptr; handler_ptr++) {
		_clltk_tracebuffer_handler_t *const handler = *handler_ptr;
		_clltk_tracebuffer_deinit(handler);
	}
}

_CLLTK_EXTERN_C_END
#endif

#define _CLLTK_STATIC_TRACEBUFFER(_NAME_, _SIZE_)                                            \
	_CLLTK_EXTERN_C_BEGIN                                                                    \
	extern const void *const __start__clltk_##_NAME_##_meta __attribute__((weak));           \
	extern const void *const __stop__clltk_##_NAME_##_meta __attribute__((weak));            \
                                                                                             \
	static _clltk_tracebuffer_handler_t _clltk_##_NAME_                                      \
		__attribute__((used)) = {{#_NAME_, _SIZE_},                                          \
								 {                                                           \
									 &__start__clltk_##_NAME_##_meta,                        \
									 &__stop__clltk_##_NAME_##_meta,                         \
								 },                                                          \
								 {NULL, _clltk_file_offset_unset}};                          \
                                                                                             \
	static _clltk_tracebuffer_handler_t *const _clltk_##_NAME_##_ptr                         \
		__attribute__((used, section("_clltk_tracebuffer_handler_ptr"))) = &_clltk_##_NAME_; \
                                                                                             \
	_CLLTK_EXTERN_C_END

#define _CLLTK_STATIC_TRACEPOINT(_BUFFER_, _FORMAT_, ...)                                        \
	do {                                                                                         \
		/* ------- compile time stuff ------- */                                                 \
                                                                                                 \
		_CLLTK_STATIC_ASSERT(_CLLTK_NARGS(__VA_ARGS__) <= 10,                                    \
							 "only supporting up to 10 arguments");                              \
		_CLLTK_CHECK_FOR_ARGUMENTS(__VA_ARGS__);                                                 \
                                                                                                 \
		/* create meta data for this tracepoint */                                               \
		/* and add them to section */                                                            \
		_CLLTK_CREATE_META_ENTRY_ARGS(_meta, _CLLTK_PLACE_IN(_BUFFER_), _FORMAT_, __VA_ARGS__);  \
                                                                                                 \
		/* create type information for va_list access at runtime. */                             \
		/* it is not possible to use meta data because there is no */                            \
		/* common meta data struct usable for all tracepoints. */                                \
		static _clltk_argument_types_t _clltk_types = _CLLTK_CREATE_TYPES(__VA_ARGS__);          \
                                                                                                 \
		static _clltk_tracebuffer_handler_t *const tb = &_clltk_##_BUFFER_;                      \
                                                                                                 \
		/* ------- runtime time stuff ------- */                                                 \
                                                                                                 \
		if ((tb->runtime.tracebuffer == NULL)) {                                                 \
			_clltk_tracebuffer_init(tb);                                                         \
		}                                                                                        \
                                                                                                 \
		static _clltk_file_offset_t _clltk_offset = _clltk_file_offset_unset;                    \
		if (_clltk_offset == _clltk_file_offset_unset) {                                         \
			_clltk_offset = _clltk_tracebuffer_get_in_file_offset(tb, &_meta, sizeof(_meta));    \
		}                                                                                        \
                                                                                                 \
		/* at runtime execute trace point */                                                     \
		_clltk_static_tracepoint_with_args(tb, _clltk_offset, __FILE__, __LINE__, &_clltk_types, \
										   _FORMAT_ _CLLTK_CAST(__VA_ARGS__));                   \
	} while (0)

#define _CLLTK_STATIC_TRACEPOINT_DUMP(_BUFFER_, _MESSAGE_, _ADDRESS_, _SIZE_)                    \
	do {                                                                                         \
		/* ------- compile time stuff ------- */                                                 \
                                                                                                 \
		/* create meta data for this tracepoint */                                               \
		/* and add them to section */                                                            \
		_CLLTK_CREATE_META_ENTRY_DUMP(_meta, _CLLTK_PLACE_IN(_BUFFER_), _MESSAGE_);              \
                                                                                                 \
		static _clltk_tracebuffer_handler_t *const tb = &_clltk_##_BUFFER_;                      \
                                                                                                 \
		/* ------- runtime time stuff ------- */                                                 \
                                                                                                 \
		if ((tb->runtime.tracebuffer == NULL)) {                                                 \
			_clltk_tracebuffer_init(tb);                                                         \
		}                                                                                        \
                                                                                                 \
		static _clltk_file_offset_t _clltk_offset = _clltk_file_offset_unset;                    \
		if (_clltk_offset == _clltk_file_offset_unset) {                                         \
			_clltk_offset = _clltk_tracebuffer_get_in_file_offset(tb, &_meta, sizeof(_meta));    \
		}                                                                                        \
                                                                                                 \
		/* at runtime execute trace point */                                                     \
		_clltk_static_tracepoint_with_dump(tb, _clltk_offset, _meta.file, _meta.line, _ADDRESS_, \
										   _SIZE_);                                              \
	} while (0)

#endif
