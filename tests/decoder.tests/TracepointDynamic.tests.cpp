// Copyright (c) 2024, International Business Machines
// SPDX-License-Identifier: BSD-2-Clause-Patent

#include "gmock/gmock.h"
#include <chrono>
#include <filesystem>
#include <memory>
#include <stddef.h>
#include <string>

#include "CommonLowLevelTracingKit/decoder/Tracebuffer.hpp"
#include "CommonLowLevelTracingKit/decoder/Tracepoint.hpp"
#include "CommonLowLevelTracingKit/tracing/tracing.h"
#include "helper.hpp"
#include "gtest/gtest.h"

#define TB CLLTK_TRACEBUFFER_MACRO_VALUE(decoder_TracepointDynamic)
using namespace testing::internal;
using namespace testing;

constexpr size_t TB_SIZE = 375000;
CLLTK_TRACEBUFFER(TB, TB_SIZE)

using namespace CommonLowLevelTracingKit::decoder;
using namespace std::string_literals;
class decoder_TracepointDynamic : public ::testing::Test
{
  protected:
	std::string m_file_name;
	std::string m_tb_name;
	void SetUp() override
	{
		m_tb_name = STR(TB);
		m_file_name = trace_file(m_tb_name);
		SETUP(TB);
	}
	void TearDown() override { CLEANUP(TB); }
};

TEST_F(decoder_TracepointDynamic, name_size_path)
{
	clltk_dynamic_tracebuffer_creation(m_tb_name.c_str(), 512);
	clltk_dynamic_tracepoint_execution(m_tb_name.c_str(), "", 0, 1, 1, "A");
	auto tb = SyncTracebuffer::make(m_file_name);
	EXPECT_EQ(tb->name(), m_tb_name.c_str());
	EXPECT_GT(tb->size(), 512);
	EXPECT_EQ(tb->path().stem(), m_tb_name);
}

TEST_F(decoder_TracepointDynamic, get_tracepoint_simple)
{
	clltk_dynamic_tracebuffer_creation(m_tb_name.c_str(), 512);
	clltk_dynamic_tracepoint_execution(m_tb_name.c_str(), "", 0, 1, 1, "A");
	auto tb = SyncTracebuffer::make(m_file_name);
	EXPECT_TRUE(tb->next());
	EXPECT_FALSE(tb->next());
}

TEST_F(decoder_TracepointDynamic, get_fileoffset)
{
	clltk_dynamic_tracebuffer_creation(m_tb_name.c_str(), 512);
	clltk_dynamic_tracepoint_execution(m_tb_name.c_str(), "", 0, 1, 1, "A");
	auto tb = SyncTracebuffer::make(m_file_name);
	auto tp = tb->next();
	EXPECT_TRUE(tp);
	EXPECT_EQ(tp->type(), Tracepoint::Type::Dynamic);
}
TEST_F(decoder_TracepointDynamic, get_filename)
{
	clltk_dynamic_tracebuffer_creation(m_tb_name.c_str(), 512);
	clltk_dynamic_tracepoint_execution(m_tb_name.c_str(), "file", 0, 1, 1, "A");
	auto tb = SyncTracebuffer::make(m_file_name);
	auto tp = tb->next();
	EXPECT_TRUE(tp);
	EXPECT_EQ(tp->file(), "file"s);
}
TEST_F(decoder_TracepointDynamic, get_line)
{
	clltk_dynamic_tracebuffer_creation(m_tb_name.c_str(), 512);
	clltk_dynamic_tracepoint_execution(m_tb_name.c_str(), "file", 42, 1, 1, "A");
	auto tb = SyncTracebuffer::make(m_file_name);
	auto tp = tb->next();
	EXPECT_TRUE(tp);
	EXPECT_EQ(tp->line(), 42);
}
TEST_F(decoder_TracepointDynamic, get_pid)
{
	clltk_dynamic_tracebuffer_creation(m_tb_name.c_str(), 512);
	clltk_dynamic_tracepoint_execution(m_tb_name.c_str(), "", 0, 1, 2, "A");
	auto tb = SyncTracebuffer::make(m_file_name);
	auto tp = tb->next();
	EXPECT_TRUE(tp);
	EXPECT_EQ(tp->pid(), 1);
}
TEST_F(decoder_TracepointDynamic, get_tid)
{
	clltk_dynamic_tracebuffer_creation(m_tb_name.c_str(), 512);
	clltk_dynamic_tracepoint_execution(m_tb_name.c_str(), "", 0, 1, 2, "A");
	auto tb = SyncTracebuffer::make(m_file_name);
	auto tp = tb->next();
	EXPECT_TRUE(tp);
	EXPECT_EQ(tp->tid(), 2);
}
TEST_F(decoder_TracepointDynamic, get_timestamp)
{
	clltk_dynamic_tracebuffer_creation(m_tb_name.c_str(), 512);
	auto now = []() {
		return std::chrono::duration_cast<std::chrono::nanoseconds>(
				   std::chrono::high_resolution_clock::now().time_since_epoch())
			.count();
	};
	auto before = now();
	clltk_dynamic_tracepoint_execution(m_tb_name.c_str(), "", 0, 1, 2, "A");
	auto after = now();
	auto tb = SyncTracebuffer::make(m_file_name);
	auto tp = tb->next();
	EXPECT_TRUE(tp);
	auto t = tp->timestamp_ns;
	EXPECT_GT(t, before);
	EXPECT_LT(t, after);
}

TEST_F(decoder_TracepointDynamic, get_msg)
{
	clltk_dynamic_tracebuffer_creation(m_tb_name.c_str(), 512);
	clltk_dynamic_tracepoint_execution(m_tb_name.c_str(), "", 0, 1, 2, "%s", "arg");
	auto tb = SyncTracebuffer::make(m_file_name);
	auto tp = tb->next();
	EXPECT_EQ(tp->msg(), "arg");
}
TEST_F(decoder_TracepointDynamic, to_big_entry)
{
	clltk_dynamic_tracebuffer_creation(m_tb_name.c_str(), UINT16_MAX + 1024);
	std::string data;
	data.resize(UINT16_MAX + 1, 'A');
	CaptureStderr();
	CLLTK_DYN_TRACEPOINT(m_tb_name.c_str(), "%s", data.data());
	EXPECT_THAT(
		GetCapturedStderr(),
		MatchesRegex(R"(.*clltk recoverable: raw entry size .* bigger than max size in.*)"));
	auto tb = SyncTracebuffer::make(m_file_name);
	EXPECT_FALSE(tb->next());
}
TEST_F(decoder_TracepointDynamic, big_entry)
{
	clltk_dynamic_tracebuffer_creation(m_tb_name.c_str(), UINT16_MAX + 1024);
	std::string data;
	data.resize(UINT16_MAX / 2, 'A');
	CLLTK_DYN_TRACEPOINT(m_tb_name.c_str(), "%s", data.data());
	auto tb = SyncTracebuffer::make(m_file_name);
	auto tp = tb->next();
	EXPECT_TRUE(tp);
	EXPECT_EQ(tp->msg(), data);
}