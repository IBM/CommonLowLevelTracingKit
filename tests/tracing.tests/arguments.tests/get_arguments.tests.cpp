// Copyright (c) 2024, International Business Machines
// SPDX-License-Identifier: BSD-2-Clause-Patent

#include "CommonLowLevelTracingKit/tracing/tracing.h"
#include "arguments.h"
#include "gtest/gtest.h"
#include <cstdint>
#include <stdarg.h>
#include <string.h>
#include <string>

void _helper(const char *const format, void *const buffer, _clltk_argument_types_t *types, ...)
{
	va_list args;
	va_start(args, types);
	uint32_t sizes[10] = {0};
	get_argument_sizes(format, sizes, types, args);
	get_arguments(buffer, sizes, types, args);
	va_end(args);
	return;
}

bool memzero(const uint8_t *const ptr, const size_t size)
{
	for (size_t offset = 0; offset < size; offset++) {
		if (ptr[offset] != 0)
			return false;
	}
	return true;
}

TEST(get_argument, types_null)
{
	_clltk_argument_types_t types = {};
	uint8_t buffer[4096] = {0};
	_helper("", buffer, &types);
	EXPECT_TRUE(memzero(buffer, sizeof(buffer)));
}

TEST(get_argument, types_str_str_str)
{
	_clltk_argument_types_t types = {
		3, 1, 0, {_clltk_argument_string, _clltk_argument_string, _clltk_argument_string}};

	uint8_t buffer[4096] = {0};
	_helper("%s %s %s", buffer, &types, "arg0", "arg1", "arg2");
	const uint8_t *ptr = buffer;

	constexpr size_t argN_size = 5;
	// arg0
	EXPECT_EQ(*(uint32_t *)ptr, argN_size);
	ptr += sizeof(uint32_t);
	EXPECT_TRUE(memcmp(ptr, "arg0", argN_size) == 0);
	ptr += argN_size;

	// arg1
	EXPECT_EQ(*(uint32_t *)ptr, argN_size);
	ptr += sizeof(uint32_t);
	EXPECT_TRUE(memcmp(ptr, "arg1", argN_size) == 0);
	ptr += argN_size;

	// arg2
	EXPECT_EQ(*(uint32_t *)ptr, argN_size);
	ptr += sizeof(uint32_t);
	EXPECT_TRUE(memcmp(ptr, "arg2", argN_size) == 0);
	ptr += argN_size;

	// remaining
	EXPECT_TRUE(memzero(ptr, sizeof(buffer) - ((uint64_t)ptr - (uint64_t)buffer)));
}

TEST(get_argument, types_str_int64_str)
{
	_clltk_argument_types_t types = {
		3, 1, 0, {_clltk_argument_string, _clltk_argument_sint64, _clltk_argument_string}};

	uint8_t buffer[4096] = {0};
	_helper("%s %u %s", buffer, &types, "some arg", 0x4d61696e6c6f6f70,
			"work/folder/source-file.cpp line:62");
	const uint8_t *ptr = buffer;

	// arg0
	const char *const arg0 = "some arg";
	const size_t arg0_n = strlen(arg0) + 1;
	EXPECT_EQ(*(uint32_t *)ptr, arg0_n);
	ptr += sizeof(uint32_t);
	EXPECT_TRUE(memcmp(ptr, arg0, arg0_n) == 0);
	ptr += arg0_n;

	// arg1
	EXPECT_EQ(0x4d61696e6c6f6f70, *(int64_t *)ptr);
	ptr += sizeof(int64_t);

	// arg2
	const char *const arg2 = "work/folder/source-file.cpp line:62";
	const size_t arg2_n = strlen(arg2) + 1;
	EXPECT_EQ(*(uint32_t *)ptr, arg2_n);
	ptr += sizeof(uint32_t);
	EXPECT_TRUE(memcmp(ptr, arg2, arg2_n) == 0);
	ptr += arg2_n;

	// remaining
	EXPECT_TRUE(memzero(ptr, sizeof(buffer) - ((uint64_t)ptr - (uint64_t)buffer)));
}

TEST(get_argument, types_str)
{
	_clltk_argument_types_t types = {1, 1, 0, {_clltk_argument_string}};

	uint8_t buffer[4096] = {0};
	_helper("%s", buffer, &types, "Mainloop");
	const uint8_t *ptr = buffer;

	// arg0
	const char *str0 = "Mainloop";
	const size_t str0_n = strlen(str0) + 1;
	EXPECT_EQ(*(uint32_t *)ptr, str0_n);
	ptr += sizeof(uint32_t);
	EXPECT_TRUE(memcmp(ptr, str0, str0_n) == 0);
	ptr += str0_n;

	// remaining
	EXPECT_TRUE(memzero(ptr, sizeof(buffer) - ((uint64_t)ptr - (uint64_t)buffer)));
}