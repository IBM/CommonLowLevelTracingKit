// Copyright (c) 2024, International Business Machines
// SPDX-License-Identifier: BSD-2-Clause-Patent

#include "CommonLowLevelTracingKit/tracing/tracing.h"
#include "gtest/gtest.h"
#include <memory>
#include <stdint.h>
#include <string.h>

CLLTK_TRACEBUFFER(META_MACRO_00, 1024)
TEST(meta_macro, str)
{
	volatile char arg0[] = "arg0 string";
	CLLTK_TRACEPOINT(META_MACRO_00, "arg0 = %s", arg0);
	const static uint32_t ref_line = __LINE__;
	const char *const meta = (const char *)_clltk_META_MACRO_00.meta.start;
	const char magic = *reinterpret_cast<const char *>(&meta[0]);
	const uint32_t size = *reinterpret_cast<const uint32_t *>(&meta[1]);
	const _clltk_meta_enty_type type = *reinterpret_cast<const _clltk_meta_enty_type *>(&meta[5]);
	const uint32_t line = *reinterpret_cast<const uint32_t *>(&meta[6]);
	const uint8_t arg_count = *reinterpret_cast<const uint8_t *>(&meta[10]);
	const char *const arg_types = reinterpret_cast<const char *>(&meta[11]);
	const char *const meta_file_name = reinterpret_cast<const char *>(&meta[10 + arg_count + 2]);
	const char *const meta_format =
		reinterpret_cast<const char *>(&meta[10 + arg_count + 2 + strlen(__FILE__) + 1]);

	EXPECT_EQ(magic, '{');
	EXPECT_EQ(size, 24 + strlen(__FILE__));
	EXPECT_EQ(type, _clltk_meta_enty_type_printf);
	EXPECT_EQ(line, ref_line - 1);
	EXPECT_EQ(arg_count, 1);
	EXPECT_EQ(arg_types[0], _clltk_argument_string);
	EXPECT_EQ(arg_types[arg_count], 0);
	EXPECT_STRCASEEQ(meta_file_name, __FILE__);
	EXPECT_STRCASEEQ(meta_format, "arg0 = %s");
}

CLLTK_TRACEBUFFER(META_MACRO_01, 1024)
TEST(meta_macro, str_str)
{
	volatile char arg0[] = "arg0 string";
	volatile char arg1[] = "arg1 string";
	CLLTK_TRACEPOINT(META_MACRO_01, "arg0 = %s arg1 = %s", arg0, arg1);
	const static uint32_t ref_line = __LINE__;
	const char *const meta = (const char *)_clltk_META_MACRO_01.meta.start;
	const char magic = *reinterpret_cast<const char *>(&meta[0]);
	const uint32_t size = *reinterpret_cast<const uint32_t *>(&meta[1]);
	const _clltk_meta_enty_type type = *reinterpret_cast<const _clltk_meta_enty_type *>(&meta[5]);
	const uint32_t line = *reinterpret_cast<const uint32_t *>(&meta[6]);
	const uint8_t arg_count = *reinterpret_cast<const uint8_t *>(&meta[10]);
	const char *const arg_types = reinterpret_cast<const char *>(&meta[11]);
	const char *const meta_file_name = reinterpret_cast<const char *>(&meta[10 + arg_count + 2]);
	const char *const meta_format =
		reinterpret_cast<const char *>(&meta[10 + arg_count + 2 + strlen(__FILE__) + 1]);

	EXPECT_EQ(magic, '{');
	EXPECT_EQ(size, 35 + strlen(__FILE__));
	EXPECT_EQ(type, _clltk_meta_enty_type_printf);
	EXPECT_EQ(line, ref_line - 1);
	EXPECT_EQ(arg_count, 2);
	EXPECT_EQ(arg_types[0], _clltk_argument_string);
	EXPECT_EQ(arg_types[1], _clltk_argument_string);
	EXPECT_EQ(arg_types[arg_count], 0);
	EXPECT_STRCASEEQ(meta_file_name, __FILE__);
	EXPECT_STRCASEEQ(meta_format, "arg0 = %s arg1 = %s");
}

CLLTK_TRACEBUFFER(META_MACRO_02, 1024)
TEST(meta_macro, int64)
{
	volatile int64_t arg0 = -1;
	CLLTK_TRACEPOINT(META_MACRO_02, "arg0 = %ld", arg0);
	const static uint32_t ref_line = __LINE__;
	const char *const meta = (const char *)_clltk_META_MACRO_02.meta.start;
	const char magic = *reinterpret_cast<const char *>(&meta[0]);
	const uint32_t size = *reinterpret_cast<const uint32_t *>(&meta[1]);
	const _clltk_meta_enty_type type = *reinterpret_cast<const _clltk_meta_enty_type *>(&meta[5]);
	const uint32_t line = *reinterpret_cast<const uint32_t *>(&meta[6]);
	const uint8_t arg_count = *reinterpret_cast<const uint8_t *>(&meta[10]);
	const char *const arg_types = reinterpret_cast<const char *>(&meta[11]);
	const char *const meta_file_name = reinterpret_cast<const char *>(&meta[10 + arg_count + 2]);
	const char *const meta_format =
		reinterpret_cast<const char *>(&meta[10 + arg_count + 2 + strlen(__FILE__) + 1]);

	EXPECT_EQ(magic, '{');
	EXPECT_EQ(size, 25 + strlen(__FILE__));
	EXPECT_EQ(type, _clltk_meta_enty_type_printf);
	EXPECT_EQ(line, ref_line - 1);
	EXPECT_EQ(arg_count, 1);
	EXPECT_EQ(arg_types[0], _clltk_argument_sint64);
	EXPECT_EQ(arg_types[arg_count], 0);
	EXPECT_STRCASEEQ(meta_file_name, __FILE__);
	EXPECT_STRCASEEQ(meta_format, "arg0 = %ld");
}

CLLTK_TRACEBUFFER(META_MACRO_03, 1024)
TEST(meta_macro, two_tracepoints)
{

	const char *meta = (const char *)_clltk_META_MACRO_03.meta.start;
	{
		volatile int64_t arg0 = -1;
		CLLTK_TRACEPOINT(META_MACRO_03, "arg0 = %ld", arg0);
		const static uint32_t ref_line = __LINE__;
		const char magic = *reinterpret_cast<const char *>(&meta[0]);
		const uint32_t size = *reinterpret_cast<const uint32_t *>(&meta[1]);
		const _clltk_meta_enty_type type =
			*reinterpret_cast<const _clltk_meta_enty_type *>(&meta[5]);
		const uint32_t line = *reinterpret_cast<const uint32_t *>(&meta[6]);
		const uint8_t arg_count = *reinterpret_cast<const uint8_t *>(&meta[10]);
		const char *const arg_types = reinterpret_cast<const char *>(&meta[11]);
		const char *const meta_file_name =
			reinterpret_cast<const char *>(&meta[10 + arg_count + 2]);
		const char *const meta_format =
			reinterpret_cast<const char *>(&meta[10 + arg_count + 2 + strlen(__FILE__) + 1]);

		EXPECT_EQ(magic, '{');
		EXPECT_EQ(size, 25 + strlen(__FILE__));
		EXPECT_EQ(type, _clltk_meta_enty_type_printf);
		EXPECT_EQ(line, ref_line - 1);
		EXPECT_EQ(arg_count, 1);
		EXPECT_EQ(arg_types[0], _clltk_argument_sint64);
		EXPECT_EQ(arg_types[arg_count], 0);
		EXPECT_STRCASEEQ(meta_file_name, __FILE__);
		EXPECT_STRCASEEQ(meta_format, "arg0 = %ld");
		meta += size;
		size_t offset = 0;
		for (offset = 0; meta[offset] == '\0'; offset++)
			;
		meta += offset;
	}
	{
		volatile int64_t arg0 = -1;
		CLLTK_TRACEPOINT(META_MACRO_03, "arg0 = %ld", arg0);
		const static uint32_t ref_line = __LINE__;
		const char magic = *reinterpret_cast<const char *>(&meta[0]);
		const uint32_t size = *reinterpret_cast<const uint32_t *>(&meta[1]);
		const _clltk_meta_enty_type type =
			*reinterpret_cast<const _clltk_meta_enty_type *>(&meta[5]);
		const uint32_t line = *reinterpret_cast<const uint32_t *>(&meta[6]);
		const uint8_t arg_count = *reinterpret_cast<const uint8_t *>(&meta[10]);
		const char *const arg_types = reinterpret_cast<const char *>(&meta[11]);
		const char *const meta_file_name =
			reinterpret_cast<const char *>(&meta[10 + arg_count + 2]);
		const char *const meta_format =
			reinterpret_cast<const char *>(&meta[10 + arg_count + 2 + strlen(__FILE__) + 1]);

		EXPECT_EQ(magic, '{');
		EXPECT_EQ(size, 25 + strlen(__FILE__));
		EXPECT_EQ(type, _clltk_meta_enty_type_printf);
		EXPECT_EQ(line, ref_line - 1);
		EXPECT_EQ(arg_count, 1);
		EXPECT_EQ(arg_types[0], _clltk_argument_sint64);
		EXPECT_EQ(arg_types[arg_count], 0);
		EXPECT_STRCASEEQ(meta_file_name, __FILE__);
		EXPECT_STRCASEEQ(meta_format, "arg0 = %ld");
		meta += size;
	}
}

CLLTK_TRACEBUFFER(META_MACRO_04, 1024)
TEST(meta_macro, three_tracepoints)
{

	const char *meta = (const char *)_clltk_META_MACRO_04.meta.start;
	{
		int64_t arg0 = -1;
		CLLTK_TRACEPOINT(META_MACRO_04, "arg0 = %ld", arg0);
		const char magic = *reinterpret_cast<const char *>(&meta[0]);
		const uint32_t size = *reinterpret_cast<const uint32_t *>(&meta[1]);

		EXPECT_EQ(magic, '{');
		EXPECT_EQ(size, 25 + strlen(__FILE__));
		meta += size;
		size_t offset = 0;
		for (offset = 0; meta[offset] == '\0'; offset++)
			;
		meta += offset;
	}
	{
		volatile char arg0[] = "Hello World!\n";
		CLLTK_TRACEPOINT(META_MACRO_04, "arg0 = %s", arg0);
		const char magic = *reinterpret_cast<const char *>(&meta[0]);
		const uint32_t size = *reinterpret_cast<const uint32_t *>(&meta[1]);

		EXPECT_EQ(magic, '{');
		meta += size;
		size_t offset = 0;
		for (offset = 0; meta[offset] == '\0'; offset++)
			;
		meta += offset;
	}
	{
		volatile double arg0 = 3e-23;
		CLLTK_TRACEPOINT(META_MACRO_04, "arg0 = %f", arg0);
		const char magic = *reinterpret_cast<const char *>(&meta[0]);
		const uint32_t size = *reinterpret_cast<const uint32_t *>(&meta[1]);

		EXPECT_EQ(magic, '{');
		meta += size;
	}
}