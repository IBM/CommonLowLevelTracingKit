// Copyright (c) 2024, International Business Machines
// SPDX-License-Identifier: BSD-2-Clause-Patent

#include <filesystem>
#include <memory>
#include <stddef.h>
#include <stdio.h>
#include <string>

#include "CommonLowLevelTracingKit/Decoder/Tracebuffer.hpp"
#include "CommonLowLevelTracingKit/tracing.h"
#include "TracepointInternal.hpp"
#include "helper.hpp"
#include "tracebufferfile.hpp"
#include "gtest/gtest.h"

#define TB CLLTK_TRACEBUFFER_MACRO_VALUE(decoder_SyncTracebuffer)
constexpr size_t TB_SIZE = 105;
CLLTK_TRACEBUFFER(TB, TB_SIZE)

using namespace CommonLowLevelTracingKit::decoder;
class decoder_SyncTracebuffer : public ::testing::Test
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
	void tp() const { TP("A"); }
};

TEST_F(decoder_SyncTracebuffer, name)
{
	auto tb = SyncTracebuffer::make(m_file_name);
	EXPECT_EQ(tb->name(), m_tb_name.c_str());
}
TEST_F(decoder_SyncTracebuffer, size)
{
	auto tb = SyncTracebuffer::make(m_file_name);
	EXPECT_GT(tb->size(), TB_SIZE);
}
TEST_F(decoder_SyncTracebuffer, path)
{
	auto tb = SyncTracebuffer::make(m_file_name);
	EXPECT_EQ(tb->path(), m_file_name);
	EXPECT_EQ(tb->path().stem(), m_tb_name);
	const Tracebuffer *const raw_tb = dynamic_cast<const Tracebuffer *>(tb.get());
	EXPECT_EQ(raw_tb->path(), m_file_name);
}
TEST_F(decoder_SyncTracebuffer, is_user_space)
{
	auto tb = SyncTracebuffer::make(m_file_name);
	EXPECT_TRUE(tb->is_user_space());
}

TEST_F(decoder_SyncTracebuffer, invalid_meta)
{
	tp();
	{
		auto raw_tb = source::TracebufferFile(m_file_name);
		auto first_entry = raw_tb.getRingbuffer().getNextEntry();
		const uint64_t fileoffset = get<uint64_t>(first_entry->body()) & ((1ULL << 48) - 1);
		const auto f = fopen(m_file_name.c_str(), "r+");
		uint8_t old = 0;
		ASSERT_TRUE(pread(fileno(f), &old, 1, (long)fileoffset));
		ASSERT_EQ(old, '{');
		const uint8_t damaged = '?';
		ASSERT_TRUE(pwrite(fileno(f), &damaged, 1, (long)fileoffset));
	} // meta should now be damaged
	auto tb = SyncTracebuffer::make(m_file_name);
	EXPECT_THROW(tb->next(), exception::InvalidMeta);
}

TEST_F(decoder_SyncTracebuffer, invalid_meta_after_get)
{
	tp();
	auto tb = SyncTracebuffer::make(m_file_name);
	auto tp = tb->next();
	{
		auto raw_tb = source::TracebufferFile(m_file_name);
		auto first_entry = raw_tb.getRingbuffer().getNextEntry();
		const uint64_t fileoffset = get<uint64_t>(first_entry->body()) & ((1ULL << 48) - 1);
		const auto f = fopen(m_file_name.c_str(), "r+");
		uint8_t old = 0;
		ASSERT_TRUE(pread(fileno(f), &old, 1, (long)fileoffset));
		ASSERT_EQ(old, '{');
		const uint8_t damaged = '?';
		ASSERT_TRUE(pwrite(fileno(f), &damaged, 1, (long)fileoffset));
	} // meta should now be damaged
	EXPECT_EQ(tp->msg(), "A");
}

#define _Atomic
#include "ringbuffer/ringbuffer.h"
#include "tracebuffer.h"
TEST_F(decoder_SyncTracebuffer, invalid_fileoffset)
{
	auto rb = _clltk_decoder_SyncTracebuffer.runtime.tracebuffer->ringbuffer;
	auto tb = SyncTracebuffer::make(m_file_name);
	{
		uint64_t data = 0xF0;
		EXPECT_EQ(ringbuffer_in(rb, &data, sizeof(data)), sizeof(data));
		EXPECT_THROW(tb->next(), exception::InvalidEntry);
	}
	{
		uint64_t data = tb->size() * 100;
		EXPECT_EQ(ringbuffer_in(rb, &data, sizeof(data)), sizeof(data));
		EXPECT_THROW(tb->next(), exception::InvalidEntry);
	}
}