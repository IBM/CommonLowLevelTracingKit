// Copyright (c) 2024, International Business Machines
// SPDX-License-Identifier: BSD-2-Clause-Patent

#ifndef _CLLTK_ARGUMENTS_H_
#define _CLLTK_ARGUMENTS_H_
// IWYU pragma: private, include "CommonLowLevelTracingKit/tracing.h"

#include "CommonLowLevelTracingKit/_macros.h"

#if defined(__KERNEL__)
#include <linux/types.h>
#else
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef enum __attribute__((__packed__)) {
	_clltk_argument_t_unknown = '?',
	_clltk_argument_t_uint8 = 'c',
	_clltk_argument_t_int8 = 'C',
	_clltk_argument_t_uint16 = 'w',
	_clltk_argument_t_int16 = 'W',
	_clltk_argument_t_uint32 = 'i',
	_clltk_argument_t_int32 = 'I',
	_clltk_argument_t_uint64 = 'l',
	_clltk_argument_t_int64 = 'L',
	_clltk_argument_t_uint128 = 'q',
	_clltk_argument_t_int128 = 'Q',
	_clltk_argument_t_float = 'f',
	_clltk_argument_t_double = 'd',
	_clltk_argument_t_string = 's',
	_clltk_argument_t_dump = 'x',
	_clltk_argument_t_pointer = 'p',
} _clltk_argument_t;

typedef struct _clltk_argument_types_t _clltk_argument_types_t;
struct _clltk_argument_types_t {
	uint8_t count;
	bool flex_size;
	bool already_checked;
	_clltk_argument_t types[10];
};
#ifdef __cplusplus
};
#endif

/// supported c type to clltk type enum
#ifndef __cplusplus // for c
#ifdef __KERNEL__
#define _CLLTK_TYPE_TO_TYPE(_I_, _X_)                                \
	_Generic((_X_),                                                  \
		_CLLTK_GENERIC_CASE(char, _clltk_argument_t_int8),           \
		_CLLTK_GENERIC_CASE(uint8_t, _clltk_argument_t_uint8),       \
		_CLLTK_GENERIC_CASE(int8_t, _clltk_argument_t_int8),         \
		_CLLTK_GENERIC_CASE(uint16_t, _clltk_argument_t_uint16),     \
		_CLLTK_GENERIC_CASE(int16_t, _clltk_argument_t_int16),       \
		_CLLTK_GENERIC_CASE(uint32_t, _clltk_argument_t_uint32),     \
		_CLLTK_GENERIC_CASE(int32_t, _clltk_argument_t_int32),       \
		_CLLTK_GENERIC_CASE(uint64_t, _clltk_argument_t_uint64),     \
		_CLLTK_GENERIC_CASE(int64_t, _clltk_argument_t_int64),       \
		_CLLTK_GENERIC_CASE(__uint128_t, _clltk_argument_t_uint128), \
		_CLLTK_GENERIC_CASE(__int128_t, _clltk_argument_t_int128),   \
		_CLLTK_GENERIC_CASE(float, _clltk_argument_t_float),         \
		_CLLTK_GENERIC_CASE(double, _clltk_argument_t_double),       \
		_CLLTK_GENERIC_CASE(void *, _clltk_argument_t_pointer),      \
		_CLLTK_GENERIC_CASE(char *, _clltk_argument_t_string),       \
		default: _clltk_argument_t_pointer) // everything else as (void*)
#else
#define _CLLTK_TYPE_TO_TYPE(_I_, _X_)                                      \
	_Generic((_X_),                                                        \
		_CLLTK_GENERIC_CASE(char, _clltk_argument_t_int8),                 \
		_CLLTK_GENERIC_CASE(uint8_t, _clltk_argument_t_uint8),             \
		_CLLTK_GENERIC_CASE(int8_t, _clltk_argument_t_int8),               \
		_CLLTK_GENERIC_CASE(uint16_t, _clltk_argument_t_uint16),           \
		_CLLTK_GENERIC_CASE(int16_t, _clltk_argument_t_int16),             \
		_CLLTK_GENERIC_CASE(uint32_t, _clltk_argument_t_uint32),           \
		_CLLTK_GENERIC_CASE(int32_t, _clltk_argument_t_int32),             \
		_CLLTK_GENERIC_CASE(uint64_t, _clltk_argument_t_uint64),           \
		_CLLTK_GENERIC_CASE(int64_t, _clltk_argument_t_int64),             \
		_CLLTK_GENERIC_CASE(unsigned long long, _clltk_argument_t_uint64), \
		_CLLTK_GENERIC_CASE(signed long long, _clltk_argument_t_int64),    \
		_CLLTK_GENERIC_CASE(__uint128_t, _clltk_argument_t_uint128),       \
		_CLLTK_GENERIC_CASE(__int128_t, _clltk_argument_t_int128),         \
		_CLLTK_GENERIC_CASE(float, _clltk_argument_t_float),               \
		_CLLTK_GENERIC_CASE(double, _clltk_argument_t_double),             \
		_CLLTK_GENERIC_CASE(void *, _clltk_argument_t_pointer),            \
		_CLLTK_GENERIC_CASE(char *, _clltk_argument_t_string),             \
		default: _clltk_argument_t_pointer) // everything else as (void*)
#endif

#else // else for c++
#include <type_traits>
template <typename T> constexpr _clltk_argument_t _CLLTK_TYPE_TO_TYPE_TEMP(void)
{
	using real_type = std::remove_cv_t<std::remove_reference_t<T>>;
	if constexpr (std::is_same<real_type, bool>::value)
		return _clltk_argument_t_uint8;
	else if constexpr (std::is_same<real_type, char>::value)
		return _clltk_argument_t_int8;
	else if constexpr (std::is_same<real_type, int8_t>::value)
		return _clltk_argument_t_int8;
	else if constexpr (std::is_same<real_type, uint8_t>::value)
		return _clltk_argument_t_uint8;
	else if constexpr (std::is_same<real_type, int8_t>::value)
		return _clltk_argument_t_int8;
	else if constexpr (std::is_same<real_type, uint16_t>::value)
		return _clltk_argument_t_uint16;
	else if constexpr (std::is_same<real_type, int16_t>::value)
		return _clltk_argument_t_int16;
	else if constexpr (std::is_same<real_type, uint32_t>::value)
		return _clltk_argument_t_uint32;
	else if constexpr (std::is_same<real_type, int32_t>::value)
		return _clltk_argument_t_int32;
	else if constexpr (std::is_same<real_type, uint64_t>::value)
		return _clltk_argument_t_uint64;
	else if constexpr (std::is_same<real_type, int64_t>::value)
		return _clltk_argument_t_int64;
	else if constexpr (std::is_same<real_type, unsigned long long>::value)
		return _clltk_argument_t_uint64;
	else if constexpr (std::is_same<real_type, signed long long>::value)
		return _clltk_argument_t_int64;
	else if constexpr (std::is_same<real_type, __uint128_t>::value)
		return _clltk_argument_t_uint128;
	else if constexpr (std::is_same<real_type, __int128_t>::value)
		return _clltk_argument_t_int128;
	else if constexpr (std::is_same<real_type, float>::value)
		return _clltk_argument_t_float;
	else if constexpr (std::is_same<real_type, double>::value)
		return _clltk_argument_t_double;
	else if constexpr (std::is_same<real_type, void *>::value)
		return _clltk_argument_t_uint64;
	else if constexpr ((std::is_pointer<real_type>::value &&
						std::is_same<std::remove_cv_t<std::remove_pointer_t<real_type>>,
									 char>::value) ||
					   (std::is_array<real_type>::value &&
						std::is_same<std::remove_cv_t<std::remove_extent_t<real_type>>,
									 char>::value))
		return _clltk_argument_t_string;
	else if constexpr (std::is_pointer<real_type>::value)
		return _clltk_argument_t_pointer;
	else if constexpr (std::is_array<real_type>::value)
		return _clltk_argument_t_pointer;
	else if constexpr (std::is_enum<real_type>::value)
		return _CLLTK_TYPE_TO_TYPE_TEMP<std::underlying_type_t<real_type>>();
	else
		return _clltk_argument_t_unknown;
}
#define _CLLTK_TYPE_TO_TYPE(_I_, _X_) _CLLTK_TYPE_TO_TYPE_TEMP<decltype(_X_)>()
#endif

// get list of all types of the arguments
#define _CLLTK_ARG_TYPES_TO_TYPES(...) _CLLTK_APPLY(_CLLTK_TYPE_TO_TYPE, _CLLTK_COMMA, __VA_ARGS__)

/// supported c type to clltk type enum
#ifndef __cplusplus // for c
#ifdef __KERNEL__
#define _CLLTK_TYPE_IS_FLEX(_I_, _X_)            \
	_Generic((_X_),                              \
		_CLLTK_GENERIC_CASE(char, false),        \
		_CLLTK_GENERIC_CASE(uint8_t, false),     \
		_CLLTK_GENERIC_CASE(int8_t, false),      \
		_CLLTK_GENERIC_CASE(uint16_t, false),    \
		_CLLTK_GENERIC_CASE(int16_t, false),     \
		_CLLTK_GENERIC_CASE(uint32_t, false),    \
		_CLLTK_GENERIC_CASE(int32_t, false),     \
		_CLLTK_GENERIC_CASE(uint64_t, false),    \
		_CLLTK_GENERIC_CASE(int64_t, false),     \
		_CLLTK_GENERIC_CASE(__uint128_t, false), \
		_CLLTK_GENERIC_CASE(__int128_t, false),  \
		_CLLTK_GENERIC_CASE(float, false),       \
		_CLLTK_GENERIC_CASE(double, false),      \
		_CLLTK_GENERIC_CASE(void *, false),      \
		_CLLTK_GENERIC_CASE(char *, true),       \
		default: false)
#else
#define _CLLTK_TYPE_IS_FLEX(_I_, _X_)                   \
	_Generic((_X_),                                     \
		_CLLTK_GENERIC_CASE(char, false),               \
		_CLLTK_GENERIC_CASE(uint8_t, false),            \
		_CLLTK_GENERIC_CASE(int8_t, false),             \
		_CLLTK_GENERIC_CASE(uint16_t, false),           \
		_CLLTK_GENERIC_CASE(int16_t, false),            \
		_CLLTK_GENERIC_CASE(uint32_t, false),           \
		_CLLTK_GENERIC_CASE(int32_t, false),            \
		_CLLTK_GENERIC_CASE(uint64_t, false),           \
		_CLLTK_GENERIC_CASE(int64_t, false),            \
		_CLLTK_GENERIC_CASE(signed long long, false),   \
		_CLLTK_GENERIC_CASE(unsigned long long, false), \
		_CLLTK_GENERIC_CASE(__uint128_t, false),        \
		_CLLTK_GENERIC_CASE(__int128_t, false),         \
		_CLLTK_GENERIC_CASE(float, false),              \
		_CLLTK_GENERIC_CASE(double, false),             \
		_CLLTK_GENERIC_CASE(void *, false),             \
		_CLLTK_GENERIC_CASE(char *, true),              \
		default: false)
#endif

#else // else for c++
#include <type_traits>
template <_clltk_argument_t arg_type> constexpr bool _CLLTK_TYPE_IS_FLEX_TEMP(void)
{
	if constexpr (arg_type == _clltk_argument_t_string)
		return true;
	else if constexpr (arg_type == _clltk_argument_t_dump)
		return true;
	else
		return false;
}
#define _CLLTK_TYPE_IS_FLEX(_I_, _X_) \
	_CLLTK_TYPE_IS_FLEX_TEMP<_CLLTK_TYPE_TO_TYPE_TEMP<decltype(_X_)>()>()
#endif

// get true if all args are fix size, and no dump or string
#define _CLLTK_ARG_TYPES_ARE_FLEX(...) \
	__VA_OPT__(_CLLTK_APPLY(_CLLTK_TYPE_IS_FLEX, _CLLTK_OR, __VA_ARGS__)) + 0

// create struct with type informations
#define _CLLTK_CREATE_TYPES(...)                                   \
	{                                                              \
		/* .count = */ _CLLTK_NARGS(__VA_ARGS__),                  \
		/* .flex_size = */ _CLLTK_ARG_TYPES_ARE_FLEX(__VA_ARGS__), \
		/* .already_checked = */ false,                            \
		/* .types = */ {_CLLTK_ARG_TYPES_TO_TYPES(__VA_ARGS__)},   \
	}

// create single static assert for one argument if it is known
#define _CLLTK_CHECK_ONE_FOR_ARGUMENTS(INDEX, VALUE)                                     \
	_CLLTK_STATIC_ASSERT(_CLLTK_TYPE_TO_TYPE(INDEX, VALUE) != _clltk_argument_t_unknown, \
						 "unknown type at position " #INDEX " from argument \"" #VALUE "\"");

// create static assert for each argument to check if type is known
#define _CLLTK_CHECK_FOR_ARGUMENTS(...) \
	_CLLTK_APPLY(_CLLTK_CHECK_ONE_FOR_ARGUMENTS, _CLLTK_NOTHING, __VA_ARGS__)

// cast any arguments which is an enum to underlying type
#define _CLLTK_CAST(...) __VA_OPT__(, _CLLTK_APPLY(_CLLTK_SINGLE_CAST, _CLLTK_COMMA, __VA_ARGS__))

static inline uint32_t _clltk_type_to_size(_clltk_argument_t type)
{
	switch (type) {
	case _clltk_argument_t_uint8:
		return sizeof(uint8_t);
	case _clltk_argument_t_int8:
		return sizeof(int8_t);
	case _clltk_argument_t_uint16:
		return sizeof(uint16_t);
	case _clltk_argument_t_int16:
		return sizeof(int16_t);
	case _clltk_argument_t_uint32:
		return sizeof(uint32_t);
	case _clltk_argument_t_int32:
		return sizeof(int32_t);
	case _clltk_argument_t_uint64:
		return sizeof(uint64_t);
	case _clltk_argument_t_int64:
		return sizeof(int64_t);
	case _clltk_argument_t_uint128:
		return sizeof(__int128_t);
	case _clltk_argument_t_int128:
		return sizeof(__uint128_t);
	case _clltk_argument_t_float:
		return sizeof(float);
	case _clltk_argument_t_double:
		return sizeof(double);
	case _clltk_argument_t_string:
		return 4;
	case _clltk_argument_t_dump:
		return 4 + 8;
	case _clltk_argument_t_pointer:
		return 8;
	case _clltk_argument_t_unknown:
		return 0;
	}
	return 0;
}

#endif
