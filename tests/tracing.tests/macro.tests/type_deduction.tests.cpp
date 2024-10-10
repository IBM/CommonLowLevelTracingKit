// Copyright (c) 2024, International Business Machines
// SPDX-License-Identifier: BSD-2-Clause-Patent

#include "CommonLowLevelTracingKit/tracing.h"
#include "gtest/gtest.h"
#include <memory>
#include <stdint.h>

TEST(type_deduction, uint8)
{
	EXPECT_EQ(_clltk_argument_t_uint8, _CLLTK_TYPE_TO_TYPE(0, static_cast<uint8_t>(0)));
}

TEST(type_deduction, int8)
{
	EXPECT_EQ(_clltk_argument_t_int8, _CLLTK_TYPE_TO_TYPE(0, static_cast<int8_t>(0)));
}

TEST(type_deduction, uint16)
{
	EXPECT_EQ(_clltk_argument_t_uint16, _CLLTK_TYPE_TO_TYPE(0, static_cast<uint16_t>(0)));
}

TEST(type_deduction, int16)
{
	EXPECT_EQ(_clltk_argument_t_int16, _CLLTK_TYPE_TO_TYPE(0, static_cast<int16_t>(0)));
}

TEST(type_deduction, uint32)
{
	EXPECT_EQ(_clltk_argument_t_uint32, _CLLTK_TYPE_TO_TYPE(0, static_cast<uint32_t>(0)));
}

TEST(type_deduction, int32)
{
	EXPECT_EQ(_clltk_argument_t_int32, _CLLTK_TYPE_TO_TYPE(0, static_cast<int32_t>(0)));
}

TEST(type_deduction, uint64)
{
	EXPECT_EQ(_clltk_argument_t_uint64, _CLLTK_TYPE_TO_TYPE(0, static_cast<uint64_t>(0)));
}

TEST(type_deduction, int64)
{
	EXPECT_EQ(_clltk_argument_t_int64, _CLLTK_TYPE_TO_TYPE(0, static_cast<int64_t>(0)));
}

TEST(type_deduction, uint128)
{
	EXPECT_EQ(_clltk_argument_t_uint128, _CLLTK_TYPE_TO_TYPE(0, static_cast<__uint128_t>(0)));
}

TEST(type_deduction, int128)
{
	EXPECT_EQ(_clltk_argument_t_int128, _CLLTK_TYPE_TO_TYPE(0, static_cast<__int128_t>(0)));
}

TEST(type_deduction, char)
{
	EXPECT_EQ(_clltk_argument_t_int8, _CLLTK_TYPE_TO_TYPE(0, static_cast<char>(0)));
}

TEST(type_deduction, unsigned_char)
{
	EXPECT_EQ(_clltk_argument_t_uint8, _CLLTK_TYPE_TO_TYPE(0, static_cast<unsigned char>(0)));
}

TEST(type_deduction, short)
{
	EXPECT_EQ(_clltk_argument_t_int16, _CLLTK_TYPE_TO_TYPE(0, static_cast<short>(0)));
}

TEST(type_deduction, unsigned_short)
{
	EXPECT_EQ(_clltk_argument_t_uint16, _CLLTK_TYPE_TO_TYPE(0, static_cast<unsigned short>(0)));
}

TEST(type_deduction, int)
{
	EXPECT_EQ(_clltk_argument_t_int32, _CLLTK_TYPE_TO_TYPE(0, static_cast<int>(0)));
}

TEST(type_deduction, unsigned_int)
{
	EXPECT_EQ(_clltk_argument_t_uint32, _CLLTK_TYPE_TO_TYPE(0, static_cast<unsigned int>(0)));
}

TEST(type_deduction, long)
{
	EXPECT_EQ(_clltk_argument_t_int64, _CLLTK_TYPE_TO_TYPE(0, static_cast<long>(0)));
}

TEST(type_deduction, unsigned_long)
{
	EXPECT_EQ(_clltk_argument_t_uint64, _CLLTK_TYPE_TO_TYPE(0, static_cast<unsigned long>(0)));
}

TEST(type_deduction, long_long)
{
	EXPECT_EQ(_clltk_argument_t_int64, _CLLTK_TYPE_TO_TYPE(0, static_cast<long long>(0)));
}

TEST(type_deduction, unsigned_long_long)
{
	EXPECT_EQ(_clltk_argument_t_uint64, _CLLTK_TYPE_TO_TYPE(0, static_cast<unsigned long long>(0)));
}

TEST(type_deduction, long_long_int)
{
	EXPECT_EQ(_clltk_argument_t_int64, _CLLTK_TYPE_TO_TYPE(0, static_cast<long long int>(0)));
}

TEST(type_deduction, unsigned_long_long_int)
{
	EXPECT_EQ(_clltk_argument_t_uint64,
			  _CLLTK_TYPE_TO_TYPE(0, static_cast<unsigned long long int>(0)));
}

TEST(type_deduction, typedef_int)
{
	typedef int my_type;
	EXPECT_EQ(_clltk_argument_t_int32, _CLLTK_TYPE_TO_TYPE(0, static_cast<my_type>(0)));
}