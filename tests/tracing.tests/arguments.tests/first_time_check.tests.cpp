// Copyright (c) 2024, International Business Machines
// SPDX-License-Identifier: BSD-2-Clause-Patent

#include "CommonLowLevelTracingKit/tracing.h"
#include "arguments.h"
#include "gtest/gtest.h"
#include <string>

TEST(first_time_check, simple_string)
{
	const char *const format = "%s";
	_clltk_argument_types_t types = {1, 1, 1, {_clltk_argument_string}};
	first_time_check(format, &types);
	EXPECT_TRUE(types.already_checked);
	EXPECT_TRUE(types.flex_size);
	EXPECT_EQ(types.types[0], _clltk_argument_string);
}

TEST(first_time_check, false_string)
{
	const char *const format = "%p";
	_clltk_argument_types_t types = {1, 1, 1, {_clltk_argument_string}};
	first_time_check(format, &types);
	EXPECT_TRUE(types.already_checked);
	EXPECT_FALSE(types.flex_size);
	EXPECT_EQ(types.types[0], _clltk_argument_pointer);
}

TEST(first_time_check, escaped_than_false_string)
{
	const char *const format = "%%s %p";
	_clltk_argument_types_t types = {1, 1, 1, {_clltk_argument_string}};
	first_time_check(format, &types);
	EXPECT_TRUE(types.already_checked);
	EXPECT_FALSE(types.flex_size);
	EXPECT_EQ(types.types[0], _clltk_argument_pointer);
}

TEST(first_time_check, many_percent_than_false_string)
{
	const char *const format = "%%%p";
	_clltk_argument_types_t types = {1, 1, 1, {_clltk_argument_string}};
	first_time_check(format, &types);
	EXPECT_TRUE(types.already_checked);
	EXPECT_FALSE(types.flex_size);
	EXPECT_EQ(types.types[0], _clltk_argument_pointer);
}