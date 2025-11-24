// Copyright (c) 2024, International Business Machines
// SPDX-License-Identifier: BSD-2-Clause-Patent
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage"
#include <gtest/gtest.h>
#include <stdint.h>
#include <stdlib.h>

#define CLLTK_FORCE_C
extern "C" {
#include "CommonLowLevelTracingKit/tracing/tracing.h"
}

TEST(arguments_c, type)
{
	EXPECT_EQ(_CLLTK_TYPE_TO_TYPE(0, (bool)1), _clltk_argument_uint8);
	EXPECT_EQ(_CLLTK_TYPE_TO_TYPE(0, (char)1), _clltk_argument_sint8);
	EXPECT_EQ(_CLLTK_TYPE_TO_TYPE(0, (uint8_t)1), _clltk_argument_uint8);
	EXPECT_EQ(_CLLTK_TYPE_TO_TYPE(0, (int8_t)1), _clltk_argument_sint8);
	EXPECT_EQ(_CLLTK_TYPE_TO_TYPE(0, (uint16_t)1), _clltk_argument_uint16);
	EXPECT_EQ(_CLLTK_TYPE_TO_TYPE(0, (int16_t)1), _clltk_argument_sint16);
	EXPECT_EQ(_CLLTK_TYPE_TO_TYPE(0, (uint32_t)1), _clltk_argument_uint32);
	EXPECT_EQ(_CLLTK_TYPE_TO_TYPE(0, (int32_t)1), _clltk_argument_sint32);
	EXPECT_EQ(_CLLTK_TYPE_TO_TYPE(0, (uint64_t)1), _clltk_argument_uint64);
	EXPECT_EQ(_CLLTK_TYPE_TO_TYPE(0, (int64_t)1), _clltk_argument_sint64);
	EXPECT_EQ(_CLLTK_TYPE_TO_TYPE(0, (unsigned long long)1), _clltk_argument_uint64);
	EXPECT_EQ(_CLLTK_TYPE_TO_TYPE(0, (signed long long)1), _clltk_argument_sint64);
	EXPECT_EQ(_CLLTK_TYPE_TO_TYPE(0, (__uint128_t)1), _clltk_argument_uint128);
	EXPECT_EQ(_CLLTK_TYPE_TO_TYPE(0, (__int128_t)1), _clltk_argument_sint128);
	EXPECT_EQ(_CLLTK_TYPE_TO_TYPE(0, (float)1), _clltk_argument_float);
	EXPECT_EQ(_CLLTK_TYPE_TO_TYPE(0, (double)1), _clltk_argument_double);
	EXPECT_EQ(_CLLTK_TYPE_TO_TYPE(0, (const char *)nullptr), _clltk_argument_string);
	EXPECT_EQ(_CLLTK_TYPE_TO_TYPE(0, (char *)nullptr), _clltk_argument_string);
	EXPECT_EQ(_CLLTK_TYPE_TO_TYPE(0, (void *)nullptr), _clltk_argument_pointer);
	EXPECT_EQ(_CLLTK_TYPE_TO_TYPE(0, "arg"), _clltk_argument_string);
	int my_array[3];
	EXPECT_EQ(_CLLTK_TYPE_TO_TYPE(0, my_array), _clltk_argument_pointer);
	enum {} my_enum;
	EXPECT_EQ(_CLLTK_TYPE_TO_TYPE(0, my_enum), _clltk_argument_pointer);
	struct {
	} my_struct;
	EXPECT_EQ(_CLLTK_TYPE_TO_TYPE(0, my_struct), _clltk_argument_pointer);
}

TEST(arguments_c, flex)
{
	EXPECT_EQ(_CLLTK_TYPE_IS_FLEX(0, (bool)1), false);
	EXPECT_EQ(_CLLTK_TYPE_IS_FLEX(0, (char)1), false);
	EXPECT_EQ(_CLLTK_TYPE_IS_FLEX(0, (uint8_t)1), false);
	EXPECT_EQ(_CLLTK_TYPE_IS_FLEX(0, (int8_t)1), false);
	EXPECT_EQ(_CLLTK_TYPE_IS_FLEX(0, (uint16_t)1), false);
	EXPECT_EQ(_CLLTK_TYPE_IS_FLEX(0, (int16_t)1), false);
	EXPECT_EQ(_CLLTK_TYPE_IS_FLEX(0, (uint32_t)1), false);
	EXPECT_EQ(_CLLTK_TYPE_IS_FLEX(0, (int32_t)1), false);
	EXPECT_EQ(_CLLTK_TYPE_IS_FLEX(0, (uint64_t)1), false);
	EXPECT_EQ(_CLLTK_TYPE_IS_FLEX(0, (int64_t)1), false);
	EXPECT_EQ(_CLLTK_TYPE_IS_FLEX(0, (__uint128_t)1), false);
	EXPECT_EQ(_CLLTK_TYPE_IS_FLEX(0, (__int128_t)1), false);
	EXPECT_EQ(_CLLTK_TYPE_IS_FLEX(0, (float)1), false);
	EXPECT_EQ(_CLLTK_TYPE_IS_FLEX(0, (double)1), false);
	EXPECT_EQ(_CLLTK_TYPE_IS_FLEX(0, (void *)1), false);
	EXPECT_EQ(_CLLTK_TYPE_IS_FLEX(0, (char *)1), true);
	struct {
	} my_struct;
	EXPECT_EQ(_CLLTK_TYPE_IS_FLEX(0, my_struct), false);
	enum {} my_enum;
	EXPECT_EQ(_CLLTK_TYPE_IS_FLEX(0, my_enum), false);
	EXPECT_EQ(_CLLTK_ARG_TYPES_ARE_FLEX(1, 1.0), false);
	EXPECT_EQ(_CLLTK_ARG_TYPES_ARE_FLEX("A"), true);
	EXPECT_EQ(_CLLTK_ARG_TYPES_ARE_FLEX('C'), false);
	EXPECT_EQ(_CLLTK_ARG_TYPES_ARE_FLEX('C', "A"), true);
	EXPECT_EQ(_CLLTK_ARG_TYPES_ARE_FLEX('C', "A", nullptr), true);
}
#endif
