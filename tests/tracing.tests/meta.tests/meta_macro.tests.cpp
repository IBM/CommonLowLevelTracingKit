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
	// the meta section holds {meta, offset-cache} pointer pairs per tracepoint
	const char *const meta = ((const char *const *)_clltk_META_MACRO_00.meta.start)[0];
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
	// the meta section holds {meta, offset-cache} pointer pairs per tracepoint
	const char *const meta = ((const char *const *)_clltk_META_MACRO_01.meta.start)[0];
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
	// the meta section holds {meta, offset-cache} pointer pairs per tracepoint
	const char *const meta = ((const char *const *)_clltk_META_MACRO_02.meta.start)[0];
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

	// the meta section holds {meta, offset-cache} pointer pairs per
	// tracepoint, in emission order
	const char *const *const meta_ptrs = (const char *const *)_clltk_META_MACRO_03.meta.start;
	const char *meta = meta_ptrs[0];
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
		meta = meta_ptrs[2];
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
	}
}

CLLTK_TRACEBUFFER(META_MACRO_SPAN, 1024)
TEST(meta_macro, span_meta_layout)
{
	clltk_span_id_t outer = CLLTK_SPAN_BEGIN(META_MACRO_SPAN, CLLTK_SPAN_NO_PARENT, "span name");
	CLLTK_SPAN_END(META_MACRO_SPAN, outer);
	EXPECT_NE(outer, (clltk_span_id_t)0);

	clltk_span_id_t second = CLLTK_SPAN_BEGIN(META_MACRO_SPAN, outer, "span name");
	CLLTK_SPAN_END(META_MACRO_SPAN, second);
	EXPECT_NE(second, (clltk_span_id_t)0);
	EXPECT_NE(second, outer);

	// the meta section holds {meta, offset-cache} pointer pairs per call site
	const char *const *const meta_ptrs = (const char *const *)_clltk_META_MACRO_SPAN.meta.start;

	{ // first call site: span begin
		const char *const meta = meta_ptrs[0];
		const _clltk_meta_enty_type type =
			*reinterpret_cast<const _clltk_meta_enty_type *>(&meta[5]);
		const uint8_t arg_count = *reinterpret_cast<const uint8_t *>(&meta[10]);
		const char *const arg_types = reinterpret_cast<const char *>(&meta[11]);
		const char *const name =
			reinterpret_cast<const char *>(&meta[10 + arg_count + 2 + strlen(__FILE__) + 1]);

		EXPECT_EQ(type, _clltk_meta_enty_type_span_begin);
		EXPECT_EQ(arg_count, 2);
		EXPECT_EQ(arg_types[0], _clltk_argument_uint64);
		EXPECT_EQ(arg_types[1], _clltk_argument_uint64);
		EXPECT_STREQ(name, "span name");
	}
	{ // second call site: span end
		const char *const meta = meta_ptrs[2];
		const _clltk_meta_enty_type type =
			*reinterpret_cast<const _clltk_meta_enty_type *>(&meta[5]);
		const uint8_t arg_count = *reinterpret_cast<const uint8_t *>(&meta[10]);
		const char *const arg_types = reinterpret_cast<const char *>(&meta[11]);

		EXPECT_EQ(type, _clltk_meta_enty_type_span_end);
		EXPECT_EQ(arg_count, 1);
		EXPECT_EQ(arg_types[0], _clltk_argument_uint64);
	}
}

CLLTK_TRACEBUFFER(META_MACRO_04, 1024)
TEST(meta_macro, three_tracepoints)
{

	// the meta section holds {meta, offset-cache} pointer pairs per
	// tracepoint, in emission order
	const char *const *const meta_ptrs = (const char *const *)_clltk_META_MACRO_04.meta.start;
	const char *meta = meta_ptrs[0];
	{
		int64_t arg0 = -1;
		CLLTK_TRACEPOINT(META_MACRO_04, "arg0 = %ld", arg0);
		const char magic = *reinterpret_cast<const char *>(&meta[0]);
		const uint32_t size = *reinterpret_cast<const uint32_t *>(&meta[1]);

		EXPECT_EQ(magic, '{');
		EXPECT_EQ(size, 25 + strlen(__FILE__));
		meta = meta_ptrs[2];
	}
	{
		volatile char arg0[] = "Hello World!\n";
		CLLTK_TRACEPOINT(META_MACRO_04, "arg0 = %s", arg0);
		const char magic = *reinterpret_cast<const char *>(&meta[0]);
		const uint32_t size = *reinterpret_cast<const uint32_t *>(&meta[1]);

		EXPECT_EQ(magic, '{');
		EXPECT_GT(size, 0u);
		meta = meta_ptrs[4];
	}
	{
		volatile double arg0 = 3e-23;
		CLLTK_TRACEPOINT(META_MACRO_04, "arg0 = %f", arg0);
		const char magic = *reinterpret_cast<const char *>(&meta[0]);
		const uint32_t size = *reinterpret_cast<const uint32_t *>(&meta[1]);

		EXPECT_EQ(magic, '{');
		EXPECT_GT(size, 0u);
	}
}