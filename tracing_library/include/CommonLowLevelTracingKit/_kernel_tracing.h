// Copyright (c) 2024, International Business Machines
// SPDX-License-Identifier: BSD-2-Clause-Patent

#ifndef _CLLTK__TRACING_H_
#define _CLLTK__TRACING_H_
// IWYU pragma: private, include "CommonLowLevelTracingKit/tracing.h"

#ifndef _CLLTK_TRACING_H_
#error "CommonLowLevelTracingKit: always use "CommonLowLevelTracingKit/tracing.h" and not this file"
#endif

#ifndef __KERNEL__
#error "use this header only in kernel"
#endif

#include "CommonLowLevelTracingKit/_arguments.h"
#include "CommonLowLevelTracingKit/_internal.h"
#include "CommonLowLevelTracingKit/_macros.h"
#include "CommonLowLevelTracingKit/_meta.h"

#include <linux/kallsyms.h>
#include <linux/types.h>

typedef struct {
	_clltk_tracebuffer_handler_t *const tracebuffer;
	bool added_to_elf;
	uint64_t in_section_offset;
	const struct {
		const void *const ptr;
		const size_t size;
	} meta;
} _clltk_kernel_meta_proxy_t;

#ifndef _CLLTK_INTERNAL

void _clltk_init_tracing_for_this_module(const struct mod_kallsyms *const);
__attribute__((constructor(102), used)) static void _clltk_constructor(void)
{
	_clltk_init_tracing_for_this_module(THIS_MODULE->kallsyms);
}

void _clltk_deinit_tracing_for_this_module(const struct mod_kallsyms *const);
__attribute__((destructor(102), used)) static void _clltk_destructor(void)
{
	_clltk_deinit_tracing_for_this_module(THIS_MODULE->kallsyms);
}

#endif

#define _CLLTK_TRACEBUFFER_MACRO_VALUE(_NAME_) _NAME_

#define _CLLTK_STATIC_TRACEBUFFER(_NAME_, _SIZE_)         \
	static _clltk_tracebuffer_handler_t _clltktb_##_NAME_ \
		__attribute__((used)) = {{#_NAME_, _SIZE_}, {0, 0}, {_clltk_file_offset_unset, 0}};

#define _CLLTK_STATIC_TRACEPOINT(_BUFFER_, _FORMAT_, ...)                                     \
	do {                                                                                      \
		/* ------- compile time stuff ------- */                                              \
                                                                                              \
		_CLLTK_STATIC_ASSERT(_CLLTK_NARGS(__VA_ARGS__) <= 10,                                 \
							 "only supporting up to 10 arguments");                           \
		_CLLTK_CHECK_FOR_ARGUMENTS(__VA_ARGS__);                                              \
                                                                                              \
		/* create meta data for this tracepoint */                                            \
		/* and add them to section */                                                         \
		_CLLTK_CREATE_META_ENTRY_ARGS(_meta, __attribute__((used)), _FORMAT_, __VA_ARGS__);   \
                                                                                              \
		/* create type information for va_list access at runtime. */                          \
		/* it is not possible to use meta data because there is no */                         \
		/* common meta data struct usable for all tracepoints. */                             \
		static _clltk_argument_types_t _clltk_types = _CLLTK_CREATE_TYPES(__VA_ARGS__);       \
                                                                                              \
		static _clltk_tracebuffer_handler_t *const tb = &_clltktb_##_BUFFER_;                 \
		static _clltk_kernel_meta_proxy_t _clltktp                                            \
			__attribute__((used)) = {.tracebuffer = tb,                                       \
									 .in_section_offset = _clltk_file_offset_unset,           \
									 .meta = {&_meta, sizeof(_meta)}};                        \
		/* ------- runtime time stuff ------- */                                              \
		static _clltk_file_offset_t _clltk_offset = _clltk_file_offset_unset;                 \
		if (_clltk_offset == _clltk_file_offset_unset) {                                      \
			_clltk_offset = tb->meta.file_offset + _clltktp.in_section_offset;                \
		}                                                                                     \
                                                                                              \
		/* at runtime execute trace point */                                                  \
		_clltk_static_tracepoint_with_args(tb, _clltk_offset, _meta.file, _meta.line,         \
										   &_clltk_types, _FORMAT_ _CLLTK_CAST(__VA_ARGS__)); \
	} while (0)

#define _CLLTK_STATIC_TRACEPOINT_DUMP(_BUFFER_, _MESSAGE_, _ADDRESS_, _SIZE_)                    \
	do {                                                                                         \
		/* ------- compile time stuff ------- */                                                 \
                                                                                                 \
		/* create meta data for this tracepoint */                                               \
		/* and add them to section */                                                            \
		_CLLTK_CREATE_META_ENTRY_DUMP(_meta, _CLLTK_PLACE_IN(_BUFFER_), _MESSAGE_);              \
                                                                                                 \
		static _clltk_tracebuffer_handler_t *const tb = &_clltktb_##_BUFFER_;                    \
		static _clltk_kernel_meta_proxy_t _clltktp                                               \
			__attribute__((used)) = {.tracebuffer = tb,                                          \
									 .in_section_offset = _clltk_file_offset_unset,              \
									 .meta = {&_meta, sizeof(_meta)}};                           \
		/* ------- runtime time stuff ------- */                                                 \
		static _clltk_file_offset_t _clltk_offset = _clltk_file_offset_unset;                    \
		if (_clltk_offset == _clltk_file_offset_unset) {                                         \
			_clltk_offset = tb->meta.file_offset + _clltktp.in_section_offset;                   \
		}                                                                                        \
                                                                                                 \
		/* at runtime execute trace point */                                                     \
		_clltk_static_tracepoint_with_dump(tb, _clltk_offset, _meta.file, _meta.line, _ADDRESS_, \
										   _SIZE_);                                              \
	} while (0)

#endif
