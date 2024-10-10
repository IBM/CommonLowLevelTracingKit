// Copyright (c) 2024, International Business Machines
// SPDX-License-Identifier: BSD-2-Clause-Patent

#include "CommonLowLevelTracingKit/tracing.h"

char _clltk_arg_test_variable_c_char = 0;
_CLLTK_STATIC_ASSERT(sizeof(_clltk_arg_test_variable_c_char) == 1,
					 "CommonLowLevelTracingKit: type deduction failed");
_CLLTK_STATIC_ASSERT(_CLLTK_TYPE_TO_TYPE(0, _clltk_arg_test_variable_c_char) ==
						 _clltk_argument_t_int8,
					 "CommonLowLevelTracingKit: type deduction failed");

signed char _clltk_arg_test_variable_c_signed_char = 0;
_CLLTK_STATIC_ASSERT(sizeof(_clltk_arg_test_variable_c_signed_char) == 1,
					 "CommonLowLevelTracingKit: type deduction failed");
_CLLTK_STATIC_ASSERT(_CLLTK_TYPE_TO_TYPE(0, _clltk_arg_test_variable_c_signed_char) ==
						 _clltk_argument_t_int8,
					 "CommonLowLevelTracingKit: type deduction failed");

unsigned char _clltk_arg_test_variable_c_unsigned_char = 0;
_CLLTK_STATIC_ASSERT(sizeof(_clltk_arg_test_variable_c_unsigned_char) == 1,
					 "CommonLowLevelTracingKit: type deduction failed");
_CLLTK_STATIC_ASSERT(_CLLTK_TYPE_TO_TYPE(0, _clltk_arg_test_variable_c_unsigned_char) ==
						 _clltk_argument_t_uint8,
					 "CommonLowLevelTracingKit: type deduction failed");

short _clltk_arg_test_variable_c_short = 0;
_CLLTK_STATIC_ASSERT(sizeof(_clltk_arg_test_variable_c_short) == 2,
					 "CommonLowLevelTracingKit: type deduction failed");
_CLLTK_STATIC_ASSERT(_CLLTK_TYPE_TO_TYPE(0, _clltk_arg_test_variable_c_short) ==
						 _clltk_argument_t_int16,
					 "CommonLowLevelTracingKit: type deduction failed");

signed short _clltk_arg_test_variable_c_signed_short = 0;
_CLLTK_STATIC_ASSERT(sizeof(_clltk_arg_test_variable_c_signed_short) == 2,
					 "CommonLowLevelTracingKit: type deduction failed");
_CLLTK_STATIC_ASSERT(_CLLTK_TYPE_TO_TYPE(0, _clltk_arg_test_variable_c_signed_short) ==
						 _clltk_argument_t_int16,
					 "CommonLowLevelTracingKit: type deduction failed");

unsigned short _clltk_arg_test_variable_c_unsigned_short = 0;
_CLLTK_STATIC_ASSERT(sizeof(_clltk_arg_test_variable_c_unsigned_short) == 2,
					 "CommonLowLevelTracingKit: type deduction failed");
_CLLTK_STATIC_ASSERT(_CLLTK_TYPE_TO_TYPE(0, _clltk_arg_test_variable_c_unsigned_short) ==
						 _clltk_argument_t_uint16,
					 "CommonLowLevelTracingKit: type deduction failed");

int _clltk_arg_test_variable_c_int = 0;
_CLLTK_STATIC_ASSERT(sizeof(_clltk_arg_test_variable_c_int) == 4,
					 "CommonLowLevelTracingKit: type deduction failed");
_CLLTK_STATIC_ASSERT(_CLLTK_TYPE_TO_TYPE(0, _clltk_arg_test_variable_c_int) ==
						 _clltk_argument_t_int32,
					 "CommonLowLevelTracingKit: type deduction failed");

signed int _clltk_arg_test_variable_c_signed_int = 0;
_CLLTK_STATIC_ASSERT(sizeof(_clltk_arg_test_variable_c_signed_int) == 4,
					 "CommonLowLevelTracingKit: type deduction failed");
_CLLTK_STATIC_ASSERT(_CLLTK_TYPE_TO_TYPE(0, _clltk_arg_test_variable_c_signed_int) ==
						 _clltk_argument_t_int32,
					 "CommonLowLevelTracingKit: type deduction failed");

unsigned int _clltk_arg_test_variable_c_unsigned_int = 0;
_CLLTK_STATIC_ASSERT(sizeof(_clltk_arg_test_variable_c_unsigned_int) == 4,
					 "CommonLowLevelTracingKit: type deduction failed");
_CLLTK_STATIC_ASSERT(_CLLTK_TYPE_TO_TYPE(0, _clltk_arg_test_variable_c_unsigned_int) ==
						 _clltk_argument_t_uint32,
					 "CommonLowLevelTracingKit: type deduction failed");

long _clltk_arg_test_variable_c_long = 0;
_CLLTK_STATIC_ASSERT(sizeof(_clltk_arg_test_variable_c_long) == 8,
					 "CommonLowLevelTracingKit: type deduction failed");
_CLLTK_STATIC_ASSERT(_CLLTK_TYPE_TO_TYPE(0, _clltk_arg_test_variable_c_long) ==
						 _clltk_argument_t_int64,
					 "CommonLowLevelTracingKit: type deduction failed");

signed long _clltk_arg_test_variable_c_signed_long = 0;
_CLLTK_STATIC_ASSERT(sizeof(_clltk_arg_test_variable_c_signed_long) == 8,
					 "CommonLowLevelTracingKit: type deduction failed");
_CLLTK_STATIC_ASSERT(_CLLTK_TYPE_TO_TYPE(0, _clltk_arg_test_variable_c_signed_long) ==
						 _clltk_argument_t_int64,
					 "CommonLowLevelTracingKit: type deduction failed");

unsigned long _clltk_arg_test_variable_c_unsigned_long = 0;
_CLLTK_STATIC_ASSERT(sizeof(_clltk_arg_test_variable_c_unsigned_long) == 8,
					 "CommonLowLevelTracingKit: type deduction failed");
_CLLTK_STATIC_ASSERT(_CLLTK_TYPE_TO_TYPE(0, _clltk_arg_test_variable_c_unsigned_long) ==
						 _clltk_argument_t_uint64,
					 "CommonLowLevelTracingKit: type deduction failed");

long long _clltk_arg_test_variable_c_long_long = 0;
_CLLTK_STATIC_ASSERT(sizeof(_clltk_arg_test_variable_c_long_long) == 8,
					 "CommonLowLevelTracingKit: type deduction failed");
_CLLTK_STATIC_ASSERT(_CLLTK_TYPE_TO_TYPE(0, _clltk_arg_test_variable_c_long_long) ==
						 _clltk_argument_t_int64,
					 "CommonLowLevelTracingKit: type deduction failed");

signed long long _clltk_arg_test_variable_c_signed_long_long = 0;
_CLLTK_STATIC_ASSERT(sizeof(_clltk_arg_test_variable_c_signed_long_long) == 8,
					 "CommonLowLevelTracingKit: type deduction failed");
_CLLTK_STATIC_ASSERT(_CLLTK_TYPE_TO_TYPE(0, _clltk_arg_test_variable_c_signed_long_long) ==
						 _clltk_argument_t_int64,
					 "CommonLowLevelTracingKit: type deduction failed");

unsigned long long _clltk_arg_test_variable_c_unsigned_long_long = 0;
_CLLTK_STATIC_ASSERT(sizeof(_clltk_arg_test_variable_c_unsigned_long_long) == 8,
					 "CommonLowLevelTracingKit: type deduction failed");
_CLLTK_STATIC_ASSERT(_CLLTK_TYPE_TO_TYPE(0, _clltk_arg_test_variable_c_unsigned_long_long) ==
						 _clltk_argument_t_uint64,
					 "CommonLowLevelTracingKit: type deduction failed");

long long int _clltk_arg_test_variable_c_long_long_int = 0;
_CLLTK_STATIC_ASSERT(sizeof(_clltk_arg_test_variable_c_long_long_int) == 8,
					 "CommonLowLevelTracingKit: type deduction failed");
_CLLTK_STATIC_ASSERT(_CLLTK_TYPE_TO_TYPE(0, _clltk_arg_test_variable_c_long_long_int) ==
						 _clltk_argument_t_int64,
					 "CommonLowLevelTracingKit: type deduction failed");

signed long long int _clltk_arg_test_variable_c_signed_long_long_int = 0;
_CLLTK_STATIC_ASSERT(sizeof(_clltk_arg_test_variable_c_signed_long_long_int) == 8,
					 "CommonLowLevelTracingKit: type deduction failed");
_CLLTK_STATIC_ASSERT(_CLLTK_TYPE_TO_TYPE(0, _clltk_arg_test_variable_c_signed_long_long_int) ==
						 _clltk_argument_t_int64,
					 "CommonLowLevelTracingKit: type deduction failed");

unsigned long long int _clltk_arg_test_variable_c_unsigned_long_long_int = 0;
_CLLTK_STATIC_ASSERT(sizeof(_clltk_arg_test_variable_c_unsigned_long_long_int) == 8,
					 "CommonLowLevelTracingKit: type deduction failed");
_CLLTK_STATIC_ASSERT(_CLLTK_TYPE_TO_TYPE(0, _clltk_arg_test_variable_c_unsigned_long_long_int) ==
						 _clltk_argument_t_uint64,
					 "CommonLowLevelTracingKit: type deduction failed");

struct {
} _clltk_arg_test_variable_c_struct = {};
// because we handled everything else as pointer in c
_CLLTK_STATIC_ASSERT(_CLLTK_TYPE_TO_TYPE(0, _clltk_arg_test_variable_c_struct) ==
						 _clltk_argument_t_pointer,
					 "CommonLowLevelTracingKit: type deduction failed");

struct {
} *_clltk_arg_test_variable_c_struct_ptr = 0;
_CLLTK_STATIC_ASSERT(_CLLTK_TYPE_TO_TYPE(0, _clltk_arg_test_variable_c_struct_ptr) ==
						 _clltk_argument_t_pointer,
					 "CommonLowLevelTracingKit: type deduction failed");

int _clltk_arg_test_variable_c_array_len_1[1] = {0};
_CLLTK_STATIC_ASSERT(_CLLTK_TYPE_TO_TYPE(0, _clltk_arg_test_variable_c_array_len_1) ==
						 _clltk_argument_t_pointer,
					 "CommonLowLevelTracingKit: type deduction failed");

int _clltk_arg_test_variable_c_array_len_0[0] = {};
_CLLTK_STATIC_ASSERT(_CLLTK_TYPE_TO_TYPE(0, _clltk_arg_test_variable_c_array_len_0) ==
						 _clltk_argument_t_pointer,
					 "CommonLowLevelTracingKit: type deduction failed");
