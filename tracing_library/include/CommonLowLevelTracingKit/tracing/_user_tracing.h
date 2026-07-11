// Copyright (c) 2024, International Business Machines
// SPDX-License-Identifier: BSD-2-Clause-Patent

#ifndef _CLLTK__TRACING_H_
#define _CLLTK__TRACING_H_
// IWYU pragma: private, include "CommonLowLevelTracingKit/tracing/tracing.h"

#ifndef _CLLTK_TRACING_H_
#error "CommonLowLevelTracingKit: always use "CommonLowLevelTracingKit/tracing/tracing.h" and not this file"
#endif

#if defined(__KERNEL__)
#error "use this header only in user space"
#endif

#include "CommonLowLevelTracingKit/tracing/_arguments.h"
#include "CommonLowLevelTracingKit/tracing/_internal.h"
#include "CommonLowLevelTracingKit/tracing/_macros.h"
#include "CommonLowLevelTracingKit/tracing/_meta.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#define _CLLTK_TRACEBUFFER_MACRO_VALUE(_NAME_) _NAME_

/* fmt-style tracepoints ({} placeholders) are C++20 only: std::format_string
 * validates the format against the argument types at compile time. */
#if defined(CLLTK_FOR_CPP) && defined(__has_include)
#if __has_include(<format>)
#include <format>
#include <type_traits>
#if defined(__cpp_lib_format)
#define _CLLTK_HAS_FMT 1
template <typename... _Args>
inline void _clltk_fmt_check(std::format_string<std::type_identity_t<_Args>...>, _Args &&...)
{
}
#endif
#endif
#endif

#if !defined(_CLLTK_INTERNAL)

_CLLTK_EXTERN_C_BEGIN

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage"
#endif

__attribute__((constructor(101), used)) static void _clltk_constructor(void)
{
	static const char _empty[]
		__attribute__((used, section("_clltk_tracebuffer_handler_ptr"))) = {};
	extern _clltk_tracebuffer_handler_t *const __start__clltk_tracebuffer_handler_ptr;
	extern _clltk_tracebuffer_handler_t *const __stop__clltk_tracebuffer_handler_ptr;
	for (_clltk_tracebuffer_handler_t *const *handler_ptr = &__start__clltk_tracebuffer_handler_ptr;
		 handler_ptr < &__stop__clltk_tracebuffer_handler_ptr; handler_ptr++) {
		_clltk_tracebuffer_handler_t *const handler = *handler_ptr;
		/* the discovery section holds pointer pairs per call site:
		 * {meta entry, file-offset cache}; the objects themselves are
		 * ordinary statics (see _CLLTK_EMIT_META_PTR) */
		const uint8_t *const *const meta_start = (const uint8_t *const *)handler->meta.start;
		const uint8_t *const *const meta_stop = (const uint8_t *const *)handler->meta.stop;
		if (meta_stop <= meta_start) {
			continue;
		}
		if (!_clltk_tracebuffer_init(handler)) {
			continue;
		}
		/* registers all entries in one batch and fills each call site's
		 * offset cache; already-resolved entries are skipped */
		_clltk_tracebuffer_register_metaptrs(handler, (const void *const *)meta_start,
											 (const void *const *)meta_stop);
	}
}

__attribute__((destructor(101), used)) static void _clltk_destructor(void)
{
	_clltk_terminate();
	extern _clltk_tracebuffer_handler_t *const __start__clltk_tracebuffer_handler_ptr;
	extern _clltk_tracebuffer_handler_t *const __stop__clltk_tracebuffer_handler_ptr;
	for (_clltk_tracebuffer_handler_t *const *handler_ptr = &__start__clltk_tracebuffer_handler_ptr;
		 handler_ptr < &__stop__clltk_tracebuffer_handler_ptr; handler_ptr++) {
		_clltk_tracebuffer_handler_t *const handler = *handler_ptr;
		_clltk_tracebuffer_deinit(handler);
	}
}

#if defined(__clang__)
#pragma clang diagnostic pop
#endif
_CLLTK_EXTERN_C_END

#endif

#define _CLLTK_STATIC_TRACEBUFFER(_NAME_, _SIZE_)                                            \
	_CLLTK_EXTERN_C_BEGIN                                                                    \
	extern const uint8_t *const __start__clltk_##_NAME_##_metaptr                            \
		__attribute__((weak, visibility("hidden")));                                         \
	extern const uint8_t *const __stop__clltk_##_NAME_##_metaptr                             \
		__attribute__((weak, visibility("hidden")));                                         \
                                                                                             \
	static _clltk_tracebuffer_handler_t _clltk_##_NAME_                                      \
		__attribute__((used)) = {{#_NAME_, _SIZE_},                                          \
								 {                                                           \
									 &__start__clltk_##_NAME_##_metaptr,                     \
									 &__stop__clltk_##_NAME_##_metaptr,                      \
								 },                                                          \
								 {NULL, _clltk_file_offset_unset}};                          \
                                                                                             \
	static _clltk_tracebuffer_handler_t *const _clltk_##_NAME_##_ptr                         \
		__attribute__((used, section("_clltk_tracebuffer_handler_ptr"))) = &_clltk_##_NAME_; \
                                                                                             \
	_CLLTK_EXTERN_C_END

/* Emit one discovery entry for a tracepoint call site into the section
 * _clltk_<BUFFER>_metaptr. Each entry is a pointer pair:
 *   [0] address of the call site's const meta object
 *   [1] address of the call site's file-offset cache, which the constructor
 *       fills during startup registration so the first tracepoint execution
 *       needs no lookup
 * An assembler data directive is used instead of __attribute__((section(...)))
 * because the attribute would place the object into the enclosing function's
 * COMDAT group inside inline functions and templates, and GCC (>= 15.2)
 * rejects mixing grouped and ungrouped sections of the same name in one
 * translation unit ("causes a section type conflict"). Assembler-emitted data
 * never joins a COMDAT group. The constraint/operand pair to print a bare
 * symbol address differs per target (each validated with -fPIC/-pie -Werror
 * at -O0..-O2). */
#if defined(__aarch64__)
#define _CLLTK_ASM_SYM_CONSTRAINT "S"
#define _CLLTK_ASM_SYM_OPERAND(_N_) "%c" _N_
#elif defined(__x86_64__)
#define _CLLTK_ASM_SYM_CONSTRAINT "Ws"
#define _CLLTK_ASM_SYM_OPERAND(_N_) "%p" _N_
#else /* s390x and other targets */
#define _CLLTK_ASM_SYM_CONSTRAINT "i"
#define _CLLTK_ASM_SYM_OPERAND(_N_) "%c" _N_
#endif

#define _CLLTK_EMIT_META_PTR(_BUFFER_, _META_, _OFFSET_)                                         \
	__asm__(".pushsection _clltk_" #_BUFFER_ "_metaptr,\"a\"\n\t"                                \
			".balign " _CLLTK_STR(                                                               \
				__SIZEOF_POINTER__) "\n\t"                                                       \
									".dc.a " _CLLTK_ASM_SYM_OPERAND(                             \
										"0") "\n\t"                                              \
											 ".dc.a " _CLLTK_ASM_SYM_OPERAND(                    \
												 "1") "\n\t"                                     \
													  ".popsection" ::_CLLTK_ASM_SYM_CONSTRAINT( \
														  &_META_),                              \
			_CLLTK_ASM_SYM_CONSTRAINT(&_OFFSET_))

#define _CLLTK_STATIC_TRACEPOINT(_BUFFER_, _FORMAT_, ...)                                         \
	do {                                                                                          \
		/* ------- compile time stuff ------- */                                                  \
                                                                                                  \
		_CLLTK_STATIC_ASSERT(_CLLTK_NARGS(__VA_ARGS__) <= 10,                                     \
							 "only supporting up to 10 arguments");                               \
		_CLLTK_CHECK_FOR_ARGUMENTS(__VA_ARGS__);                                                  \
                                                                                                  \
		/* create meta data for this tracepoint, the per-call-site offset      */                 \
		/* cache (filled by the startup registration), and a discovery entry  */                  \
		static _clltk_file_offset_t _clltk_offset = _clltk_file_offset_unset;                     \
		_CLLTK_CREATE_META_ENTRY_ARGS(_meta, _CLLTK_PLACE_IN(_BUFFER_), _FORMAT_, __VA_ARGS__);   \
		_CLLTK_EMIT_META_PTR(_BUFFER_, _meta, _clltk_offset);                                     \
                                                                                                  \
		/* create type information for va_list access at runtime. */                              \
		/* it is not possible to use meta data because there is no */                             \
		/* common meta data struct usable for all tracepoints. */                                 \
		static _clltk_argument_types_t _clltk_types = _CLLTK_CREATE_TYPES(__VA_ARGS__);           \
                                                                                                  \
		static _clltk_tracebuffer_handler_t *const _tb = &_clltk_##_BUFFER_;                      \
                                                                                                  \
		/* ------- runtime time stuff ------- */                                                  \
                                                                                                  \
		if ((_tb->runtime.tracebuffer == NULL)) {                                                 \
			if (!_clltk_tracebuffer_init(_tb)) {                                                  \
				break;                                                                            \
			}                                                                                     \
		}                                                                                         \
                                                                                                  \
		/* normally already set by the constructor; fallback for call sites   */                  \
		/* executed before startup registration (constructor priority <= 101) */                  \
		_clltk_file_offset_t _clltk_off = __atomic_load_n(&_clltk_offset, __ATOMIC_RELAXED); \
		if (_clltk_off == _clltk_file_offset_unset) { \
			_clltk_off = _clltk_tracebuffer_get_in_file_offset(_tb, &_meta, sizeof(_meta)); \
			__atomic_store_n(&_clltk_offset, _clltk_off, __ATOMIC_RELAXED); \
		}                                                                                         \
                                                                                                  \
		/* at runtime execute trace point */                                                      \
		_clltk_static_tracepoint_with_args(_tb, _clltk_off, __FILE__, __LINE__, &_clltk_types, \
										   _FORMAT_ _CLLTK_CAST(__VA_ARGS__));                    \
	} while (0)

#if defined(_CLLTK_HAS_FMT)
#define _CLLTK_STATIC_TRACEPOINT_FMT(_BUFFER_, _FORMAT_, ...)                                  \
	do {                                                                                       \
		/* ------- compile time stuff ------- */                                               \
		if (false) { /* never executed: validates {} format against arg types */               \
			_clltk_fmt_check(_FORMAT_ __VA_OPT__(, ) __VA_ARGS__);                             \
		}                                                                                      \
		_CLLTK_STATIC_ASSERT(_CLLTK_NARGS(__VA_ARGS__) <= 10,                                  \
							 "only supporting up to 10 arguments");                            \
		_CLLTK_CHECK_FOR_ARGUMENTS(__VA_ARGS__);                                               \
                                                                                               \
		/* create meta data for this tracepoint, the per-call-site offset      */              \
		/* cache (filled by the startup registration), and a discovery entry  */               \
		static _clltk_file_offset_t _clltk_offset = _clltk_file_offset_unset;                  \
		_CLLTK_CREATE_META_ENTRY_TYPED(_meta, _CLLTK_PLACE_IN(_BUFFER_),                       \
									   _clltk_meta_enty_type_fmt, _FORMAT_, __VA_ARGS__);      \
		_CLLTK_EMIT_META_PTR(_BUFFER_, _meta, _clltk_offset);                                  \
                                                                                               \
		static _clltk_argument_types_t _clltk_types = _CLLTK_CREATE_TYPES(__VA_ARGS__);        \
                                                                                               \
		static _clltk_tracebuffer_handler_t *const _tb = &_clltk_##_BUFFER_;                   \
                                                                                               \
		/* ------- runtime time stuff ------- */                                               \
                                                                                               \
		if ((_tb->runtime.tracebuffer == NULL)) {                                              \
			if (!_clltk_tracebuffer_init(_tb)) {                                               \
				break;                                                                         \
			}                                                                                  \
		}                                                                                      \
                                                                                               \
		_clltk_file_offset_t _clltk_off = __atomic_load_n(&_clltk_offset, __ATOMIC_RELAXED); \
		if (_clltk_off == _clltk_file_offset_unset) { \
			_clltk_off = _clltk_tracebuffer_get_in_file_offset(_tb, &_meta, sizeof(_meta)); \
			__atomic_store_n(&_clltk_offset, _clltk_off, __ATOMIC_RELAXED); \
		}                                                                                      \
                                                                                               \
		_clltk_static_tracepoint_with_args_unchecked(_tb, _clltk_off, __FILE__, __LINE__,   \
													 &_clltk_types,                            \
													 _FORMAT_ _CLLTK_CAST(__VA_ARGS__));       \
	} while (0)
#else
#define _CLLTK_STATIC_TRACEPOINT_FMT(_BUFFER_, _FORMAT_, ...)                         \
	do {                                                                              \
		_CLLTK_STATIC_ASSERT(0, "CLLTK_TRACEPOINT_FMT requires C++20 with <format>"); \
	} while (0)
#endif

/* shared compile-time core for span events: meta entry with the given type,
 * discovery entry, tracebuffer init, and resolved file offset. The
 * placeholder arguments only determine the argument type array of the meta
 * entry; the real values are passed to the runtime call by the caller. */
#define _CLLTK_STATIC_SPAN_EVENT(_TYPE_, _BUFFER_, _NAME_, _CALL_, ...)                        \
	do {                                                                                       \
		static _clltk_file_offset_t _clltk_offset = _clltk_file_offset_unset;                  \
		_CLLTK_CREATE_META_ENTRY_TYPED(_meta, _CLLTK_PLACE_IN(_BUFFER_), _TYPE_, _NAME_,       \
									   __VA_ARGS__);                                           \
		_CLLTK_EMIT_META_PTR(_BUFFER_, _meta, _clltk_offset);                                  \
                                                                                               \
		static _clltk_tracebuffer_handler_t *const _tb = &_clltk_##_BUFFER_;                   \
		if ((_tb->runtime.tracebuffer == NULL)) {                                              \
			if (!_clltk_tracebuffer_init(_tb)) {                                               \
				break;                                                                         \
			}                                                                                  \
		}                                                                                      \
		_clltk_file_offset_t _clltk_off = __atomic_load_n(&_clltk_offset, __ATOMIC_RELAXED); \
		if (_clltk_off == _clltk_file_offset_unset) { \
			_clltk_off = _clltk_tracebuffer_get_in_file_offset(_tb, &_meta, sizeof(_meta)); \
			__atomic_store_n(&_clltk_offset, _clltk_off, __ATOMIC_RELAXED); \
		}                                                                                      \
		_CALL_;                                                                                \
	} while (0)

#define _CLLTK_STATIC_SPAN_BEGIN(_BUFFER_, _PARENT_, _NAME_)                                  \
	({                                                                                        \
		const clltk_span_id_t _clltk_span_id = clltk_next_span_id();                          \
		const clltk_span_id_t _clltk_span_parent = (_PARENT_);                                \
		_CLLTK_STATIC_SPAN_EVENT(_clltk_meta_enty_type_span_begin, _BUFFER_, _NAME_,          \
								 _clltk_static_tracepoint_span_begin(                         \
									 _tb, _clltk_off, _clltk_span_id, _clltk_span_parent), \
								 (clltk_span_id_t)0, (clltk_span_id_t)0);                     \
		_clltk_span_id;                                                                       \
	})

#define _CLLTK_STATIC_SPAN_END(_BUFFER_, _ID_)                                     \
	do {                                                                           \
		const clltk_span_id_t _clltk_span_id = (_ID_);                             \
		_CLLTK_STATIC_SPAN_EVENT(                                                  \
			_clltk_meta_enty_type_span_end, _BUFFER_, "",                          \
			_clltk_static_tracepoint_span_end(_tb, _clltk_off, _clltk_span_id), \
			(clltk_span_id_t)0);                                                   \
	} while (0)

#define _CLLTK_STATIC_TRACEPOINT_DUMP(_BUFFER_, _MESSAGE_, _ADDRESS_, _SIZE_)                     \
	do {                                                                                          \
		/* ------- compile time stuff ------- */                                                  \
                                                                                                  \
		/* create meta data for this tracepoint, the per-call-site offset      */                 \
		/* cache (filled by the startup registration), and a discovery entry  */                  \
		static _clltk_file_offset_t _clltk_offset = _clltk_file_offset_unset;                     \
		_CLLTK_CREATE_META_ENTRY_DUMP(_meta, _CLLTK_PLACE_IN(_BUFFER_), _MESSAGE_);               \
		_CLLTK_EMIT_META_PTR(_BUFFER_, _meta, _clltk_offset);                                     \
                                                                                                  \
		static _clltk_tracebuffer_handler_t *const _tb = &_clltk_##_BUFFER_;                      \
                                                                                                  \
		/* ------- runtime time stuff ------- */                                                  \
                                                                                                  \
		if ((_tb->runtime.tracebuffer == NULL)) {                                                 \
			if (!_clltk_tracebuffer_init(_tb)) {                                                  \
				break;                                                                            \
			}                                                                                     \
		}                                                                                         \
                                                                                                  \
		/* normally already set by the constructor; fallback for call sites   */                  \
		/* executed before startup registration (constructor priority <= 101) */                  \
		_clltk_file_offset_t _clltk_off = __atomic_load_n(&_clltk_offset, __ATOMIC_RELAXED); \
		if (_clltk_off == _clltk_file_offset_unset) { \
			_clltk_off = _clltk_tracebuffer_get_in_file_offset(_tb, &_meta, sizeof(_meta)); \
			__atomic_store_n(&_clltk_offset, _clltk_off, __ATOMIC_RELAXED); \
		}                                                                                         \
                                                                                                  \
		/* at runtime execute trace point */                                                      \
		_clltk_static_tracepoint_with_dump(_tb, _clltk_off, _meta.file, _meta.line, _ADDRESS_, \
										   _SIZE_);                                               \
	} while (0)

#endif
