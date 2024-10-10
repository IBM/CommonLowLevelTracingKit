// Copyright (c) 2024, International Business Machines
// SPDX-License-Identifier: BSD-2-Clause-Patent

#include "CommonLowLevelTracingKit/tracing.h"
#include "arguments.h"
#include "gtest/gtest.h"
#include <cstdint>
#include <memory>
#include <stdarg.h>

uint32_t _helper(const char *const format, uint32_t sizes_out[], _clltk_argument_types_t *types,
				 ...)
{
	va_list args;
	va_start(args, types);
	const uint32_t size = get_argument_sizes(format, sizes_out, types, args);
	va_end(args);
	return size;
}

TEST(get_argument_sizes, types_null)
{
	EXPECT_EQ(0, _helper("", nullptr, nullptr));
}

TEST(get_argument_sizes, types_empty)
{
	_clltk_argument_types_t types = {};
	uint32_t sizes[1] = {0};
	EXPECT_EQ(0, _helper("", sizes, &types));
	EXPECT_EQ(0, sizes[0]);
}

TEST(get_argument_sizes, types_empty_but_flex_size)
{
	_clltk_argument_types_t types = {.count = 0, .flex_size = 1, .already_checked = 0, .types = {}};
	uint32_t sizes[1] = {0};
	EXPECT_EQ(0, _helper("", sizes, &types));
	EXPECT_EQ(0, sizes[0]);
}

TEST(get_argument_sizes, one_uint8_t)
{
	static _clltk_argument_types_t types = {
		.count = 1, .flex_size = 0, .already_checked = 0, .types = {_clltk_argument_t_uint8}};
	uint32_t sizes[1] = {0};
	EXPECT_EQ(sizeof(uint8_t), _helper("%u", sizes, &types));
	EXPECT_EQ(sizeof(uint8_t), sizes[0]);
}

TEST(get_argument_sizes, one_int8_t)
{
	static _clltk_argument_types_t types = {
		.count = 1, .flex_size = 0, .already_checked = 0, .types = {_clltk_argument_t_int8}};
	uint32_t sizes[1] = {0};
	EXPECT_EQ(sizeof(int8_t), _helper("%u", sizes, &types));
	EXPECT_EQ(sizeof(int8_t), sizes[0]);
}

TEST(get_argument_sizes, one_uint16_t)
{
	static _clltk_argument_types_t types = {
		.count = 1, .flex_size = 0, .already_checked = 0, .types = {_clltk_argument_t_uint16}};
	uint32_t sizes[1] = {0};
	EXPECT_EQ(sizeof(uint16_t), _helper("%u", sizes, &types));
	EXPECT_EQ(sizeof(uint16_t), sizes[0]);
}

TEST(get_argument_sizes, one_int16_t)
{
	static _clltk_argument_types_t types = {
		.count = 1, .flex_size = 0, .already_checked = 0, .types = {_clltk_argument_t_int16}};
	uint32_t sizes[1] = {0};
	EXPECT_EQ(sizeof(int16_t), _helper("%u", sizes, &types));
	EXPECT_EQ(sizeof(int16_t), sizes[0]);
}

TEST(get_argument_sizes, one_uint32_t)
{
	static _clltk_argument_types_t types = {
		.count = 1, .flex_size = 0, .already_checked = 0, .types = {_clltk_argument_t_uint32}};
	uint32_t sizes[1] = {0};
	EXPECT_EQ(sizeof(uint32_t), _helper("%u", sizes, &types));
	EXPECT_EQ(sizeof(uint32_t), sizes[0]);
}

TEST(get_argument_sizes, one_int32_t)
{
	static _clltk_argument_types_t types = {
		.count = 1, .flex_size = 0, .already_checked = 0, .types = {_clltk_argument_t_int32}};
	uint32_t sizes[1] = {0};
	EXPECT_EQ(sizeof(int32_t), _helper("%u", sizes, &types));
	EXPECT_EQ(sizeof(int32_t), sizes[0]);
}

TEST(get_argument_sizes, one_uint64_t)
{
	static _clltk_argument_types_t types = {
		.count = 1, .flex_size = 0, .already_checked = 0, .types = {_clltk_argument_t_uint64}};
	uint32_t sizes[1] = {0};
	EXPECT_EQ(sizeof(uint64_t), _helper("%u", sizes, &types));
	EXPECT_EQ(sizeof(uint64_t), sizes[0]);
}

TEST(get_argument_sizes, one_int64_t)
{
	static _clltk_argument_types_t types = {
		.count = 1, .flex_size = 0, .already_checked = 0, .types = {_clltk_argument_t_int64}};
	uint32_t sizes[1] = {0};
	EXPECT_EQ(sizeof(int64_t), _helper("%u", sizes, &types));
	EXPECT_EQ(sizeof(int64_t), sizes[0]);
}

TEST(get_argument_sizes, one_uint128_t)
{
	static _clltk_argument_types_t types = {
		.count = 1, .flex_size = 0, .already_checked = 0, .types = {_clltk_argument_t_uint128}};
	uint32_t sizes[1] = {0};
	EXPECT_EQ(sizeof(__uint128_t), _helper("%u", sizes, &types));
	EXPECT_EQ(sizeof(__uint128_t), sizes[0]);
}

TEST(get_argument_sizes, one_int128_t)
{
	static _clltk_argument_types_t types = {
		.count = 1, .flex_size = 0, .already_checked = 0, .types = {_clltk_argument_t_int128}};
	uint32_t sizes[1] = {0};
	EXPECT_EQ(sizeof(__int128_t), _helper("%u", sizes, &types));
	EXPECT_EQ(sizeof(__int128_t), sizes[0]);
}

TEST(get_argument_sizes, one_float)
{
	static _clltk_argument_types_t types = {
		.count = 1, .flex_size = 0, .already_checked = 0, .types = {_clltk_argument_t_float}};
	uint32_t sizes[1] = {0};
	EXPECT_EQ(sizeof(float), _helper("%u", sizes, &types));
	EXPECT_EQ(sizeof(float), sizes[0]);
}

TEST(get_argument_sizes, one_double)
{
	static _clltk_argument_types_t types = {
		.count = 1, .flex_size = 0, .already_checked = 0, .types = {_clltk_argument_t_double}};
	uint32_t sizes[1] = {0};
	EXPECT_EQ(sizeof(double), _helper("%u", sizes, &types));
	EXPECT_EQ(sizeof(double), sizes[0]);
}

TEST(get_argument_sizes, string_null)
{
	static _clltk_argument_types_t types = {
		.count = 1, .flex_size = 1, .already_checked = 0, .types = {_clltk_argument_t_string}};
	uint32_t sizes[1] = {0};
	EXPECT_EQ(5, _helper("%s", sizes, &types, nullptr));
	EXPECT_EQ(5, sizes[0]);
}

TEST(get_argument_sizes, string_empty)
{
	static _clltk_argument_types_t types = {
		.count = 1, .flex_size = 1, .already_checked = 0, .types = {_clltk_argument_t_string}};
	uint32_t sizes[1] = {0};
	EXPECT_EQ(5, _helper("%s", sizes, &types, ""));
	EXPECT_EQ(5, sizes[0]);
}

TEST(get_argument_sizes, string_1)
{
	static _clltk_argument_types_t types = {
		.count = 1, .flex_size = 1, .already_checked = 0, .types = {_clltk_argument_t_string}};
	uint32_t sizes[1] = {0};
	EXPECT_EQ(6, _helper("%s", sizes, &types, "A"));
	EXPECT_EQ(6, sizes[0]);
}

TEST(get_argument_sizes, string_2)
{
	static _clltk_argument_types_t types = {
		.count = 1, .flex_size = 1, .already_checked = 0, .types = {_clltk_argument_t_string}};
	uint32_t sizes[1] = {0};
	EXPECT_EQ(7, _helper("%s", sizes, &types, "AB"));
	EXPECT_EQ(7, sizes[0]);
}

TEST(get_argument_sizes, string_1024)
{
	static _clltk_argument_types_t types = {
		.count = 1, .flex_size = 1, .already_checked = 0, .types = {_clltk_argument_t_string}};
	char str[1025] = "";
	for (uint32_t i = 0; i < sizeof(str); i++)
		str[i] = 'A';
	str[sizeof(str) - 1] = '\0';
	uint32_t sizes[1] = {0};
	EXPECT_EQ(sizeof(str) + 4, _helper("%s", sizes, &types, str));
	EXPECT_EQ(sizeof(str) + 4, sizes[0]);
}

TEST(get_argument_sizes, int_float_string)
{
	static _clltk_argument_types_t types = {.count = 3,
											.flex_size = 1,
											.already_checked = 0,
											.types = {
												_clltk_argument_t_int64,
												_clltk_argument_t_float,
												_clltk_argument_t_string,
											}};
	int64_t arg0 = 0;
	float arg1 = 0;
	char arg2[11] = "          ";
	uint32_t sizes[3] = {0};
	EXPECT_EQ(8 + 5 + 6 + 8, _helper("%u %f %s", sizes, &types, arg0, arg1, arg2));
	EXPECT_EQ(8, sizes[0]);
	EXPECT_EQ(4, sizes[1]);
	EXPECT_EQ(4 + sizeof(arg2), sizes[2]);
}

TEST(get_argument_sizes, int_string_float)
{
	static _clltk_argument_types_t types = {.count = 3,
											.flex_size = 1,
											.already_checked = 0,
											.types = {
												_clltk_argument_t_int64,
												_clltk_argument_t_string,
												_clltk_argument_t_float,
											}};
	int64_t arg0 = 0;
	char arg1[11] = "          ";
	float arg2 = 0;
	uint32_t sizes[3] = {0};
	EXPECT_EQ(8 + 5 + 6 + 8, _helper("%u %s %f", sizes, &types, arg0, arg1, arg2));
	EXPECT_EQ(8, sizes[0]);
	EXPECT_EQ(4 + sizeof(arg1), sizes[1]);
	EXPECT_EQ(4, sizes[2]);
}

TEST(get_argument_sizes, string_int_float)
{
	static _clltk_argument_types_t types = {.count = 3,
											.flex_size = 1,
											.already_checked = 0,
											.types = {
												_clltk_argument_t_string,
												_clltk_argument_t_int64,
												_clltk_argument_t_float,
											}};
	char arg0[11] = "          ";
	int64_t arg1 = 0;
	float arg2 = 0;
	uint32_t sizes[3] = {0};
	EXPECT_EQ(4 + sizeof(arg0) + 8 + 4, _helper("%s %u %f", sizes, &types, arg0, arg1, arg2));
	EXPECT_EQ(4 + sizeof(arg0), sizes[0]);
	EXPECT_EQ(8, sizes[1]);
	EXPECT_EQ(4, sizes[2]);
}

TEST(get_argument_sizes, int_float)
{
	static _clltk_argument_types_t types = {.count = 2,
											.flex_size = 1,
											.already_checked = 0,
											.types = {
												_clltk_argument_t_int64,
												_clltk_argument_t_float,
											}};
	int64_t arg0 = 0;
	float arg1 = 0;
	uint32_t sizes[2] = {0};
	EXPECT_EQ(8 + 4, _helper("%u %f", sizes, &types, arg0, arg1));
	EXPECT_EQ(8, sizes[0]);
	EXPECT_EQ(4, sizes[1]);
}

TEST(get_argument_sizes, float_int)
{
	static _clltk_argument_types_t types = {.count = 2,
											.flex_size = 1,
											.already_checked = 0,
											.types = {
												_clltk_argument_t_float,
												_clltk_argument_t_int64,
											}};
	float arg0 = 0;
	int64_t arg1 = 0;
	uint32_t sizes[2] = {0};
	EXPECT_EQ(8 + 4, _helper("%f %u", sizes, &types, arg0, arg1));
	EXPECT_EQ(4, sizes[0]);
	EXPECT_EQ(8, sizes[1]);
}