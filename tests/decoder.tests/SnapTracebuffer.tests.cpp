// Copyright (c) 2024, International Business Machines
// SPDX-License-Identifier: BSD-2-Clause-Patent
#include <filesystem>
#include <iostream>
#include <memory>
#include <stddef.h>
#include <string>
#include <string_view>
#include <utility>

#include "CommonLowLevelTracingKit/Decoder/Tracebuffer.hpp"
#include "CommonLowLevelTracingKit/Decoder/Tracepoint.hpp"
#include "CommonLowLevelTracingKit/snapshot.hpp"
#include "CommonLowLevelTracingKit/tracing.h"
#include "helper.hpp"
#include "gtest/gtest.h"
#include <fstream>
#include <random>

using namespace CommonLowLevelTracingKit::snapshot;
using namespace CommonLowLevelTracingKit::decoder;
using namespace std::string_literals;
using namespace std::string_view_literals;

#define TB CLLTK_TRACEBUFFER_MACRO_VALUE(decoder_SnapTracebuffer)
constexpr size_t TB_SIZE = 375000;
CLLTK_TRACEBUFFER(decoder_SnapTracebuffer, TB_SIZE)

class decoder_SnapTracebuffer : public ::testing::Test
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
	TracepointPtr get_tp()
	{

		auto tb = SnapTracebuffer::make(m_file_name);
		auto *tps = const_cast<TracepointCollection *>(&tb->tracepoints);
		EXPECT_EQ(tps->size(), 1);
		return std::move(tps->front());
	}
};

TEST_F(decoder_SnapTracebuffer, empty)
{
	EXPECT_EQ(SnapTracebuffer::make(m_file_name)->tracepoints.size(), 0);
	TP("Hello World");
	EXPECT_EQ(SnapTracebuffer::make(m_file_name)->tracepoints.size(), 1);
}

TEST_F(decoder_SnapTracebuffer, name)
{
	TP("Hello World");
	auto tb = SnapTracebuffer::make(m_file_name);
	EXPECT_EQ(tb->name(), m_tb_name);
}

TEST_F(decoder_SnapTracebuffer, size)
{
	TP("Hello World");
	auto tb = SnapTracebuffer::make(m_file_name);
	EXPECT_GT(tb->size(), TB_SIZE);
	EXPECT_LT(tb->size(), TB_SIZE * 2);
}

TEST_F(decoder_SnapTracebuffer, one_static_tracepoint)
{
	TP("Hello World");
	auto tb = SnapTracebuffer::make(m_file_name);
	auto &tps = tb->tracepoints;
	EXPECT_EQ(tps.size(), 1);
	auto &tp = tps.at(0);
	EXPECT_EQ(tp->msg(), "Hello World"sv);
	EXPECT_GE(tp->timestamp_str().size(), 20);
}

TEST_F(decoder_SnapTracebuffer, two_static_tracepoint)
{
	for (int i = 0; i < 2; i++) TP("Hello World");
	auto tb = SnapTracebuffer::make(m_file_name);
	auto &tps = tb->tracepoints;
	EXPECT_EQ(tps.size(), 2);
	EXPECT_EQ(tps.at(0)->msg(), "Hello World"sv);
	EXPECT_EQ(tps.at(1)->msg(), "Hello World"sv);
}

TEST_F(decoder_SnapTracebuffer, big_static_tracepoint_print)
{
	ASSERT_GT(TB_SIZE, 16 * 1024);
	std::string data;
	data.resize(16 * 1024, 'A');
	TP("%s", data.data());

	auto tb = SnapTracebuffer::make(m_file_name);
	auto &tps = tb->tracepoints;
	EXPECT_EQ(tps.size(), 1);
	auto &tp = tps.at(0);
	EXPECT_EQ(tp->msg(), data);
}
TEST_F(decoder_SnapTracebuffer, big_static_tracepoint_dump)
{
	ASSERT_GT(TB_SIZE, 16 * 1024);
	std::string data;
	data.resize(16 * 1024, 'A');
	DUMP("data", data.data(), (uint32_t)data.size());

	auto tb = SnapTracebuffer::make(m_file_name);
	auto &tps = tb->tracepoints;
	EXPECT_EQ(tps.size(), 1);
	auto &tp = tps.at(0);
	EXPECT_GT(tp->msg().size(), data.size() * 3);
}

TEST_F(decoder_SnapTracebuffer, many_static_tracepoint)
{
	constexpr size_t tp_count = 10000;
	for (size_t i = 0; i < tp_count; i++) TP("%lu", i);
	{
		auto tb = SyncTracebuffer::make(m_file_name);
		const double fill_quota =
			static_cast<double>(tb->pending()) / static_cast<double>(tb->size());
		std::cout << "fill level = " << tb->pending() << "/" << tb->size() << " " << fill_quota
				  << std::endl;
		EXPECT_GT(fill_quota, 0.9);
	}
	auto tb = SnapTracebuffer::make(m_file_name);
	auto &tps = tb->tracepoints;
	ASSERT_EQ(tps.size(), tp_count);
	for (size_t i = 0; i < tp_count; i++) {
		EXPECT_EQ(tps.at(i)->msg(), std::to_string(i));
		EXPECT_EQ(tps.at(i)->nr, i);
	}
}

TEST_F(decoder_SnapTracebuffer, pointer_should_be_string)
{
	volatile char str[] = "some string";
#pragma GCC diagnostic ignored "-Wformat"
	TP("should be a string, not a pointer = %s %c", (volatile void *)str, 'A');
#pragma GCC diagnostic warning "-Wformat"
	auto tb = SnapTracebuffer::make(m_file_name);
	auto &tps = tb->tracepoints;
	EXPECT_EQ(tps[0]->msg().back(), 'A');
}

TEST_F(decoder_SnapTracebuffer, string_should_be_pointer)
{
	volatile char str[] = "some string";
	TP("should be a pointer, not a string = %p %c", str, 'A');
	auto tb = SnapTracebuffer::make(m_file_name);
	auto &tps = tb->tracepoints;
	EXPECT_EQ(tps[0]->msg().back(), 'A');
}
TEST_F(decoder_SnapTracebuffer, dump)
{
	volatile char str[] = "\xBE\xEF\xAB\xCD\xEF";
	DUMP("TOKEN", (void *)str, sizeof(str));
	EXPECT_TRUE(SnapTracebuffer::is_formattable(m_file_name));
	auto tb = SnapTracebuffer::make(m_file_name);
	auto &tps = tb->tracepoints;
	EXPECT_EQ(tps[0]->msg(), "TOKEN =(dump)= \"BE EF AB CD EF 00\""s);
}

TEST_F(decoder_SnapTracebuffer, uncompressed_archive)
{
	std::random_device rd;
	std::mt19937_64 gen(rd());
	std::uniform_int_distribution<uint64_t> dist(0, UINT64_MAX);
	const uint64_t value = dist(gen);
	const std::string msg = std::to_string(value);
	std::cout << "msg = " << msg << std::endl;
	TP("%s", msg.c_str());
	static const std::string archive_name = "snapshot";
	{
		std::ofstream file(archive_name, std::ios::binary | std::ios::trunc);
		const auto func = [&](const void *d, size_t s) -> std::optional<size_t> {
			file.write(std::bit_cast<char *>(d), static_cast<std::streamsize>(s));
			return s;
		};
		take_snapshot(func);
	}
	EXPECT_TRUE(SnapTracebuffer::is_formattable(archive_name));
	auto tbs = SnapTracebuffer::collect(archive_name);
	EXPECT_FALSE(tbs.empty());
	EXPECT_GE(tbs.size(), 1); // there could be some old trace buffers
	size_t found = 0;
	for (const auto &tb : tbs) {
		std::cout << "tb = " << tb->name() << std::endl;
		for (const auto &tp : tb->tracepoints) {
			found += (tp->msg() == msg);
			std::cout << "    tp->msg() = " << tp->msg() << std::endl;
		}
	}
	EXPECT_EQ(found, 1);
}

TEST_F(decoder_SnapTracebuffer, compressed_archive)
{
	std::random_device rd;
	std::mt19937_64 gen(rd());
	std::uniform_int_distribution<uint64_t> dist(0, UINT64_MAX);
	const uint64_t value = dist(gen);
	const std::string msg = std::to_string(value);
	TP("%s", msg.c_str());
	static const std::string archive_name = "snapshot";
	{
		std::ofstream file(archive_name, std::ios::binary | std::ios::trunc);
		const auto func = [&](const void *d, size_t s) -> std::optional<size_t> {
			file.write(std::bit_cast<char *>(d), static_cast<std::streamsize>(s));
			return s;
		};
		take_snapshot_compressed(func);
	}
	EXPECT_TRUE(SnapTracebuffer::is_formattable(archive_name));
	auto tbs = SnapTracebuffer::collect(archive_name);
	EXPECT_FALSE(tbs.empty());
	EXPECT_GE(tbs.size(), 1); // there could be some old trace buffers
	size_t found = 0;
	for (const auto &tb : tbs) {
		for (const auto &tp : tb->tracepoints) { found += (tp->msg() == msg); }
	}
	EXPECT_EQ(found, 1);
}
