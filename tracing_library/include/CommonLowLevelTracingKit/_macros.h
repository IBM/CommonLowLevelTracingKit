// Copyright (c) 2024, International Business Machines
// SPDX-License-Identifier: BSD-2-Clause-Patent

#ifndef _CLLTK_MACROS_H_
#define _CLLTK_MACROS_H_
// IWYU pragma: private, include "CommonLowLevelTracingKit/tracing.h"

#ifdef __cplusplus
#define _CLLTK_EXTERN_C_BEGIN extern "C" {
#define _CLLTK_EXTERN_C_END }
#else
#define _CLLTK_EXTERN_C_BEGIN
#define _CLLTK_EXTERN_C_END
#endif

#define _CLLTK_TYPEOF(_value_) __typeof__((_value_))
#define _CLLTK_MEMBER_SIZE(st, member) sizeof((((st *)0)->member))
#define _CLLTK_OFFSETOF(st, member) offsetof(st, member)

#if defined(__KERNEL__)
#define _CLLTK_PLACE_IN(_NAME_)
#else
#define _CLLTK_PLACE_IN(_NAME_) __attribute__((used, section("_clltk_" #_NAME_ "_meta")))
#endif

#ifdef __cplusplus
// C++

#define _CLLTK_STATIC_ASSERT(VALUE, MESSAGE) static_assert(VALUE, MESSAGE)

#include <type_traits>
template <typename T>
__attribute__((always_inline)) static inline constexpr auto _clltk_cast(T value)
{
	if constexpr (std::is_enum<T>::value)
		return static_cast<typename std::underlying_type<T>::type>(value);
	else
		return value;
}
#define _CLLTK_SINGLE_CAST(_INDEX_, _VALUE_) _clltk_cast(_VALUE_)

#else
// C
#define _CLLTK_STATIC_ASSERT(VALUE, MESSAGE) _Static_assert(VALUE, MESSAGE)

#define _CLLTK_SINGLE_CAST(_INDEX_, _VALUE_) _VALUE_

#endif

// https://stackoverflow.com/a/11920694

/* This counts the number of args */
#define _CLLTK_NARGS_SEQ(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, \
						 _17, _18, _19, _20, N, ...)                                            \
	N
#define _CLLTK_NARGS(...)                                                                        \
	_CLLTK_NARGS_SEQ(__VA_OPT__(__VA_ARGS__, ) 20, 19, 18, 17, 16, 15, 14, 13, 12, 11, 10, 9, 8, \
					 7, 6, 5, 4, 3, 2, 1, 0)

/* This will let macros expand before concating them */
#define _CLLTK_PRIMITIVE_CAT(x, y) x##y
#define _CLLTK_CAT(x, y) _CLLTK_PRIMITIVE_CAT(x, y)

#define _CLLTK_COMMA() ,
#define _CLLTK_AND() &&
#define _CLLTK_OR() ||
#define _CLLTK_NOTHING()

/**
 * @brief
 * @param transform a callable macro with two arguments.
 * First arg index of arg, starting with 0.
 * Second arg is value for __VA_ARGS__
 * @param sep a callable macro with out any argument.
 * Will be called between each macro call
 */
#define _CLLTK_APPLY(transform, sep, ...) \
	_CLLTK_CAT(_CLLTK_APPLY_, _CLLTK_NARGS(__VA_ARGS__))(transform, sep, __VA_ARGS__)

#define _CLLTK_APPLY_0(t, s, ...)

#define _CLLTK_APPLY_1(t, s, x0) t(0, x0)

#define _CLLTK_APPLY_2(t, s, x0, x1) t(0, x0) s() t(1, x1)

#define _CLLTK_APPLY_3(t, s, x0, x1, x2) t(0, x0) s() t(1, x1) s() t(2, x2)

#define _CLLTK_APPLY_4(t, s, x0, x1, x2, x3) t(0, x0) s() t(1, x1) s() t(2, x2) s() t(3, x3)

#define _CLLTK_APPLY_5(t, s, x0, x1, x2, x3, x4) \
	t(0, x0) s() t(1, x1) s() t(2, x2) s() t(3, x3) s() t(4, x4)

#define _CLLTK_APPLY_6(t, s, x0, x1, x2, x3, x4, x5) \
	t(0, x0) s() t(1, x1) s() t(2, x2) s() t(3, x3) s() t(4, x4) s() t(5, x5)

#define _CLLTK_APPLY_7(t, s, x0, x1, x2, x3, x4, x5, x6) \
	t(0, x0) s() t(1, x1) s() t(2, x2) s() t(3, x3) s() t(4, x4) s() t(5, x5) s() t(6, x6)

#define _CLLTK_APPLY_8(t, s, x0, x1, x2, x3, x4, x5, x6, x7)                                   \
	t(0, x0) s() t(1, x1) s() t(2, x2) s() t(3, x3) s() t(4, x4) s() t(5, x5) s() t(6, x6) s() \
		t(7, x7)

#define _CLLTK_APPLY_9(t, s, x0, x1, x2, x3, x4, x5, x6, x7, x8)                               \
	t(0, x0) s() t(1, x1) s() t(2, x2) s() t(3, x3) s() t(4, x4) s() t(5, x5) s() t(6, x6) s() \
		t(7, x7) s() t(8, x8)

#define _CLLTK_APPLY_10(t, s, x0, x1, x2, x3, x4, x5, x6, x7, x8, x9)                          \
	t(0, x0) s() t(1, x1) s() t(2, x2) s() t(3, x3) s() t(4, x4) s() t(5, x5) s() t(6, x6) s() \
		t(7, x7) s() t(8, x8) s() t(9, x9)

#ifndef __cplusplus // only for c

#if defined(__clang__)
#define _CLLTK_GENERIC_CASE(_KEY_, _VALUE_) \
	_KEY_:                                  \
	_VALUE_
#elif defined(__GNUC__)
#define _CLLTK_GENERIC_CASE(_KEY_, _VALUE_)         \
	/* because `_Generic` differentiated between:*/ \
	/* const and none-const we need both cases*/    \
	_KEY_:                                          \
	_VALUE_, const _KEY_ : _VALUE_
#endif

#endif

#endif
