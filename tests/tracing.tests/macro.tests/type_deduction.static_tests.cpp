// Copyright (c) 2024, International Business Machines
// SPDX-License-Identifier: BSD-2-Clause-Patent

#include "CommonLowLevelTracingKit/tracing/tracing.h"

char _clltk_arg_test_variable_cpp_char = 0;
_CLLTK_STATIC_ASSERT(sizeof(_clltk_arg_test_variable_cpp_char) == 1,
					 "CommonLowLevelTracingKit: type deduction failed");
_CLLTK_STATIC_ASSERT(_CLLTK_TYPE_TO_TYPE(0, _clltk_arg_test_variable_cpp_char) ==
						 _clltk_argument_sint8,
					 "CommonLowLevelTracingKit: type deduction failed");

signed char _clltk_arg_test_variable_cpp_signed_char = 0;
_CLLTK_STATIC_ASSERT(sizeof(_clltk_arg_test_variable_cpp_signed_char) == 1,
					 "CommonLowLevelTracingKit: type deduction failed");
_CLLTK_STATIC_ASSERT(_CLLTK_TYPE_TO_TYPE(0, _clltk_arg_test_variable_cpp_signed_char) ==
						 _clltk_argument_sint8,
					 "CommonLowLevelTracingKit: type deduction failed");

unsigned char _clltk_arg_test_variable_cpp_unsigned_char = 0;
_CLLTK_STATIC_ASSERT(sizeof(_clltk_arg_test_variable_cpp_unsigned_char) == 1,
					 "CommonLowLevelTracingKit: type deduction failed");
_CLLTK_STATIC_ASSERT(_CLLTK_TYPE_TO_TYPE(0, _clltk_arg_test_variable_cpp_unsigned_char) ==
						 _clltk_argument_uint8,
					 "CommonLowLevelTracingKit: type deduction failed");

short _clltk_arg_test_variable_cpp_short = 0;
_CLLTK_STATIC_ASSERT(sizeof(_clltk_arg_test_variable_cpp_short) == 2,
					 "CommonLowLevelTracingKit: type deduction failed");
_CLLTK_STATIC_ASSERT(_CLLTK_TYPE_TO_TYPE(0, _clltk_arg_test_variable_cpp_short) ==
						 _clltk_argument_sint16,
					 "CommonLowLevelTracingKit: type deduction failed");

signed short _clltk_arg_test_variable_cpp_signed_short = 0;
_CLLTK_STATIC_ASSERT(sizeof(_clltk_arg_test_variable_cpp_signed_short) == 2,
					 "CommonLowLevelTracingKit: type deduction failed");
_CLLTK_STATIC_ASSERT(_CLLTK_TYPE_TO_TYPE(0, _clltk_arg_test_variable_cpp_signed_short) ==
						 _clltk_argument_sint16,
					 "CommonLowLevelTracingKit: type deduction failed");

unsigned short _clltk_arg_test_variable_cpp_unsigned_short = 0;
_CLLTK_STATIC_ASSERT(sizeof(_clltk_arg_test_variable_cpp_unsigned_short) == 2,
					 "CommonLowLevelTracingKit: type deduction failed");
_CLLTK_STATIC_ASSERT(_CLLTK_TYPE_TO_TYPE(0, _clltk_arg_test_variable_cpp_unsigned_short) ==
						 _clltk_argument_uint16,
					 "CommonLowLevelTracingKit: type deduction failed");

int _clltk_arg_test_variable_cpp_int = 0;
_CLLTK_STATIC_ASSERT(sizeof(_clltk_arg_test_variable_cpp_int) == 4,
					 "CommonLowLevelTracingKit: type deduction failed");
_CLLTK_STATIC_ASSERT(_CLLTK_TYPE_TO_TYPE(0, _clltk_arg_test_variable_cpp_int) ==
						 _clltk_argument_sint32,
					 "CommonLowLevelTracingKit: type deduction failed");

signed int _clltk_arg_test_variable_cpp_signed_int = 0;
_CLLTK_STATIC_ASSERT(sizeof(_clltk_arg_test_variable_cpp_signed_int) == 4,
					 "CommonLowLevelTracingKit: type deduction failed");
_CLLTK_STATIC_ASSERT(_CLLTK_TYPE_TO_TYPE(0, _clltk_arg_test_variable_cpp_signed_int) ==
						 _clltk_argument_sint32,
					 "CommonLowLevelTracingKit: type deduction failed");

unsigned int _clltk_arg_test_variable_cpp_unsigned_int = 0;
_CLLTK_STATIC_ASSERT(sizeof(_clltk_arg_test_variable_cpp_unsigned_int) == 4,
					 "CommonLowLevelTracingKit: type deduction failed");
_CLLTK_STATIC_ASSERT(_CLLTK_TYPE_TO_TYPE(0, _clltk_arg_test_variable_cpp_unsigned_int) ==
						 _clltk_argument_uint32,
					 "CommonLowLevelTracingKit: type deduction failed");

long _clltk_arg_test_variable_cpp_long = 0;
_CLLTK_STATIC_ASSERT(sizeof(_clltk_arg_test_variable_cpp_long) == 8,
					 "CommonLowLevelTracingKit: type deduction failed");
_CLLTK_STATIC_ASSERT(_CLLTK_TYPE_TO_TYPE(0, _clltk_arg_test_variable_cpp_long) ==
						 _clltk_argument_sint64,
					 "CommonLowLevelTracingKit: type deduction failed");

signed long _clltk_arg_test_variable_cpp_signed_long = 0;
_CLLTK_STATIC_ASSERT(sizeof(_clltk_arg_test_variable_cpp_signed_long) == 8,
					 "CommonLowLevelTracingKit: type deduction failed");
_CLLTK_STATIC_ASSERT(_CLLTK_TYPE_TO_TYPE(0, _clltk_arg_test_variable_cpp_signed_long) ==
						 _clltk_argument_sint64,
					 "CommonLowLevelTracingKit: type deduction failed");

unsigned long _clltk_arg_test_variable_cpp_unsigned_long = 0;
_CLLTK_STATIC_ASSERT(sizeof(_clltk_arg_test_variable_cpp_unsigned_long) == 8,
					 "CommonLowLevelTracingKit: type deduction failed");
_CLLTK_STATIC_ASSERT(_CLLTK_TYPE_TO_TYPE(0, _clltk_arg_test_variable_cpp_unsigned_long) ==
						 _clltk_argument_uint64,
					 "CommonLowLevelTracingKit: type deduction failed");

long long _clltk_arg_test_variable_cpp_long_long = 0;
_CLLTK_STATIC_ASSERT(sizeof(_clltk_arg_test_variable_cpp_long_long) == 8,
					 "CommonLowLevelTracingKit: type deduction failed");
_CLLTK_STATIC_ASSERT(_CLLTK_TYPE_TO_TYPE(0, _clltk_arg_test_variable_cpp_long_long) ==
						 _clltk_argument_sint64,
					 "CommonLowLevelTracingKit: type deduction failed");

signed long long _clltk_arg_test_variable_cpp_signed_long_long = 0;
_CLLTK_STATIC_ASSERT(sizeof(_clltk_arg_test_variable_cpp_signed_long_long) == 8,
					 "CommonLowLevelTracingKit: type deduction failed");
_CLLTK_STATIC_ASSERT(_CLLTK_TYPE_TO_TYPE(0, _clltk_arg_test_variable_cpp_signed_long_long) ==
						 _clltk_argument_sint64,
					 "CommonLowLevelTracingKit: type deduction failed");

unsigned long long _clltk_arg_test_variable_cpp_unsigned_long_long = 0;
_CLLTK_STATIC_ASSERT(sizeof(_clltk_arg_test_variable_cpp_unsigned_long_long) == 8,
					 "CommonLowLevelTracingKit: type deduction failed");
_CLLTK_STATIC_ASSERT(_CLLTK_TYPE_TO_TYPE(0, _clltk_arg_test_variable_cpp_unsigned_long_long) ==
						 _clltk_argument_uint64,
					 "CommonLowLevelTracingKit: type deduction failed");

long long int _clltk_arg_test_variable_cpp_long_long_int = 0;
_CLLTK_STATIC_ASSERT(sizeof(_clltk_arg_test_variable_cpp_long_long_int) == 8,
					 "CommonLowLevelTracingKit: type deduction failed");
_CLLTK_STATIC_ASSERT(_CLLTK_TYPE_TO_TYPE(0, _clltk_arg_test_variable_cpp_long_long_int) ==
						 _clltk_argument_sint64,
					 "CommonLowLevelTracingKit: type deduction failed");

signed long long int _clltk_arg_test_variable_cpp_signed_long_long_int = 0;
_CLLTK_STATIC_ASSERT(sizeof(_clltk_arg_test_variable_cpp_signed_long_long_int) == 8,
					 "CommonLowLevelTracingKit: type deduction failed");
_CLLTK_STATIC_ASSERT(_CLLTK_TYPE_TO_TYPE(0, _clltk_arg_test_variable_cpp_signed_long_long_int) ==
						 _clltk_argument_sint64,
					 "CommonLowLevelTracingKit: type deduction failed");

unsigned long long int _clltk_arg_test_variable_cpp_unsigned_long_long_int = 0;
_CLLTK_STATIC_ASSERT(sizeof(_clltk_arg_test_variable_cpp_unsigned_long_long_int) == 8,
					 "CommonLowLevelTracingKit: type deduction failed");
_CLLTK_STATIC_ASSERT(_CLLTK_TYPE_TO_TYPE(0, _clltk_arg_test_variable_cpp_unsigned_long_long_int) ==
						 _clltk_argument_uint64,
					 "CommonLowLevelTracingKit: type deduction failed");

struct {
} _clltk_arg_test_variable_cpp_struct = {};
_CLLTK_STATIC_ASSERT(_CLLTK_TYPE_TO_TYPE(0, _clltk_arg_test_variable_cpp_struct) ==
						 _clltk_argument_unknown,
					 "CommonLowLevelTracingKit: type deduction failed");

struct {
} *_clltk_arg_test_variable_cpp_struct_ptr = 0;
_CLLTK_STATIC_ASSERT(_CLLTK_TYPE_TO_TYPE(0, _clltk_arg_test_variable_cpp_struct_ptr) ==
						 _clltk_argument_pointer,
					 "CommonLowLevelTracingKit: type deduction failed");

int _clltk_arg_test_variable_cpp_array_len_1[1] = {0};
_CLLTK_STATIC_ASSERT(_CLLTK_TYPE_TO_TYPE(0, _clltk_arg_test_variable_cpp_array_len_1) ==
						 _clltk_argument_pointer,
					 "CommonLowLevelTracingKit: type deduction failed");

int _clltk_arg_test_variable_cpp_array_len_0[0] = {};
_CLLTK_STATIC_ASSERT(_CLLTK_TYPE_TO_TYPE(0, _clltk_arg_test_variable_cpp_array_len_0) ==
						 _clltk_argument_unknown,
					 "CommonLowLevelTracingKit: type deduction failed");
