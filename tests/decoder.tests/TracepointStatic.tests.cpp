// Copyright (c) 2024, International Business Machines
// SPDX-License-Identifier: BSD-2-Clause-Patent

#include <algorithm>
#include <array>
#include <chrono>
#include <compare>
#include <fcntl.h>
#include <filesystem>
#include <iostream>
#include <latch>
#include <map>
#include <memory>
#include <sstream>
#include <stdint.h>
#include <stdlib.h>
#include <string>
#include <sys/syscall.h>
#include <thread>
#include <unistd.h>
#include <utility>
#include <vector>

#include "CommonLowLevelTracingKit/Decoder/Tracebuffer.hpp"
#include "CommonLowLevelTracingKit/Decoder/Tracepoint.hpp"
#include "CommonLowLevelTracingKit/tracing.h"
#include "helper.hpp"
#include "to_string.hpp"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
using ::testing::MatchesRegex;
using namespace CommonLowLevelTracingKit::decoder::source::low_level;

#define TB CLLTK_TRACEBUFFER_MACRO_VALUE(decoder_TracepointStatic)
constexpr size_t TB_SIZE = 64;
CLLTK_TRACEBUFFER(TB, TB_SIZE)

using namespace CommonLowLevelTracingKit::decoder;
using namespace std::string_literals;
using namespace std::chrono;
class decoder_TracepointStatic : public ::testing::Test
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

TEST_F(decoder_TracepointStatic, name_size_path)
{
	auto tb = SyncTracebuffer::make(m_file_name);
	CLLTK_TRACEPOINT(TB, "A");
	EXPECT_EQ(tb->name(), m_tb_name.c_str());
	EXPECT_GT(tb->size(), 96);
	EXPECT_EQ(tb->path().stem(), m_tb_name);
}

TEST_F(decoder_TracepointStatic, get_tracepoint_simple)
{
	auto tb = SyncTracebuffer::make(m_file_name);
	CLLTK_TRACEPOINT(TB, "A");
	EXPECT_TRUE(tb->next());
}

TEST_F(decoder_TracepointStatic, get_fileoffset)
{
	auto tb = SyncTracebuffer::make(m_file_name);
	CLLTK_TRACEPOINT(TB, "A");
	auto tp = tb->next();
	ASSERT_TRUE(tp);
	EXPECT_EQ(tp->type(), Tracepoint::Type::Static);
}
TEST_F(decoder_TracepointStatic, get_filename)
{
	auto tb = SyncTracebuffer::make(m_file_name);
	CLLTK_TRACEPOINT(TB, "A");
	auto tp = tb->next();
	ASSERT_TRUE(tp);
	EXPECT_EQ(tp->file(), __FILE__);
}
TEST_F(decoder_TracepointStatic, get_line)
{
	auto tb = SyncTracebuffer::make(m_file_name);
	CLLTK_TRACEPOINT(TB, "A");
	const size_t line = __LINE__ - 1;
	auto tp = tb->next();
	ASSERT_TRUE(tp);
	EXPECT_EQ(tp->line(), line);
}
TEST_F(decoder_TracepointStatic, get_pid)
{
	auto tb = SyncTracebuffer::make(m_file_name);
	CLLTK_TRACEPOINT(TB, "A");
	auto fun = []() {
		CLLTK_TRACEPOINT(TB, "A");
		exit(42);
	};
	EXPECT_EXIT(fun(), ::testing::ExitedWithCode(42), ".*");
	auto tpA = tb->next();
	ASSERT_TRUE(tpA);
	EXPECT_EQ(tpA->pid(), getpid());
	std::this_thread::sleep_for(std::chrono::milliseconds(10));
	auto tpB = tb->next();
	ASSERT_TRUE(tpB);
	EXPECT_NE(tpB->pid(), getpid());
}
TEST_F(decoder_TracepointStatic, get_tid)
{
	auto tb = SyncTracebuffer::make(m_file_name);
	ASSERT_FALSE(tb->next());
	CLLTK_TRACEPOINT(TB, "A");
	auto tpA = tb->next();
	ASSERT_TRUE(tpA);
	EXPECT_EQ(tpA->tid(), syscall(SYS_gettid));
	std::thread t{[]() { CLLTK_TRACEPOINT(TB, "B"); }};
	t.join();
	auto tpB = tb->next();
	ASSERT_TRUE(tpB);
	EXPECT_NE(tpB->tid(), syscall(SYS_gettid));
}
TEST_F(decoder_TracepointStatic, get_timestamp)
{
	auto tb = SyncTracebuffer::make(m_file_name);
	auto now = []() {
		return std::chrono::duration_cast<std::chrono::nanoseconds>(
				   std::chrono::high_resolution_clock::now().time_since_epoch())
			.count();
	};
	auto before = now();
	CLLTK_TRACEPOINT(TB, "A");
	auto after = now();
	auto tp = tb->next();
	ASSERT_TRUE(tp);
	auto t = tp->timestamp_ns;
	EXPECT_GT(t, before);
	EXPECT_LT(t, after);
	auto time = tp->timestamp_str();
	EXPECT_THAT(time, MatchesRegex(R"([0-9]+\.[0-9]{9})"));
	auto date = tp->date_and_time_str();
	EXPECT_THAT(date, MatchesRegex(R"([0-9]{4}-[0-9]{2}-[0-9]{2} [0-9:\.]*)"));
	EXPECT_THAT(date, MatchesRegex(R"([0-9\-]* [0-9]{2}:[0-9]{2}:[0-9]{2}\.[0-9]{9})"));
}

TEST_F(decoder_TracepointStatic, get_msg)
{
	auto tb = SyncTracebuffer::make(m_file_name);
	{
		CLLTK_TRACEPOINT(TB, "A");
		auto tp = tb->next();
		ASSERT_TRUE(tp);
		EXPECT_EQ(tp->msg(), "A"s);
	}
	{
		CLLTK_TRACEPOINT(TB, "%s", "A");
		auto tp = tb->next();
		ASSERT_TRUE(tp);
		EXPECT_EQ(tp->msg(), "A"s);
	}
	{
		CLLTK_TRACEPOINT(TB, "%c %c %c %c %c %c %c %c %c %c", '1', '2', '3', '4', '5', '6', '7',
						 '8', '9', 'A');
		auto tp = tb->next();
		ASSERT_TRUE(tp);
		EXPECT_EQ(tp->msg(), "1 2 3 4 5 6 7 8 9 A"s);
	}
	{
		CLLTK_TRACEPOINT(TB, "%3.1f", 1.5);
		auto tp = tb->next();
		ASSERT_TRUE(tp);
		EXPECT_EQ(tp->msg(), "1.5"s);
	}
}

TEST_F(decoder_TracepointStatic, access_after_tb_close)
{
	CLLTK_TRACEPOINT(TB, "%6.2f %s", 3.14, "arg");
	TracepointPtr tp{};
	{
		auto tb = SyncTracebuffer::make(m_file_name);
		tp = tb->next();
	}
	tp->msg();
}

TEST_F(decoder_TracepointStatic, read_parallel_overwhelmed)
{
	auto tb = SyncTracebuffer::make(m_file_name);
	constexpr size_t n_threads = 100;
	constexpr size_t n_tp_per_thread = 2500;
	constexpr size_t n_tp_total = n_threads * n_tp_per_thread;
	std::latch l{n_threads + 1};
	const auto write_thread_func = [&l](const size_t n_max) {
		l.arrive_and_wait();
		for (uint64_t i = 0; i < n_max; i++) {
			CLLTK_TRACEPOINT(TB, "%lu", i);
			std::this_thread::sleep_for(microseconds(1));
		}
	};

	ASSERT_FALSE(tb->next());
	std::array<std::thread, n_threads> threads;
	std::for_each(threads.begin(), threads.end(),
				  [&](std::thread &t) { t = std::thread{write_thread_func, n_tp_per_thread}; });

	std::vector<uint64_t> nrs_vector{};
	nrs_vector.reserve((n_tp_total * 12) / 10);
	l.arrive_and_wait();
	auto lastMsg = steady_clock::now();
	while ((steady_clock::now() - lastMsg) < milliseconds(100)) {
		const auto tp = tb->next();
		if (tp == nullptr) continue;
		tp->msg();
		lastMsg = steady_clock::now();
		nrs_vector.push_back(tp->nr);
	}
	std::for_each(threads.begin(), threads.end(), [](std::thread &t) { t.join(); });
	EXPECT_FALSE(tb->next()) << "there should be no new tracepoints";

	std::map<uint64_t, uint64_t> frequency;
	for (size_t num : nrs_vector) { frequency[num]++; }
	std::stringstream msg;
	size_t duplicates = 0;
	for (const auto &entry : frequency) {
		if (entry.second > 1) {
			duplicates += 1;
			msg << entry.first << " x " << entry.second << std::endl;
		}
	}
	if (!msg.str().empty()) ADD_FAILURE() << msg.str();

	std::cout << "" << frequency.size() + duplicates << "/" << nrs_vector.size() << std::endl;
	std::cout << "read total  = " << nrs_vector.size() << "/" << n_tp_total << std::endl;
	std::cout << "read unique = " << frequency.size() << "/" << n_tp_total << std::endl;
	EXPECT_LE(nrs_vector.size(), n_tp_total);
	std::cout << tb->path() << std::endl;

	const double quota = static_cast<double>(nrs_vector.size()) * 100 / n_tp_total;
	std::cout << "read = " << nrs_vector.size() << "/" << n_tp_total << "(" << quota << ")"
			  << std::endl;
	EXPECT_GT(quota, 25.0);
	EXPECT_LE(quota, 100.0);
}

TEST_F(decoder_TracepointStatic, get_msg_damages_8sec)
{
	std::string expect{};
	{ // tracepoint in tracebuffer
		uint8_t arg0 = 126;
		int16_t arg1 = -512;
		uint16_t arg2 = 1024;
		double arg3 = 3e-10;
		float arg4 = 9.9e-30F;
		char arg5[] = "ABCDEFG";
		uint16_t arg6 = 10000;
		int16_t arg7 = -10000;
		double arg8 = -1.11;
		void *arg9 = std::bit_cast<void *>(42lu);
		TP(" %u %o %x %g %f %s %X %d %e %p", arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8,
		   arg9);
		char *expect_raw = nullptr;
		EXPECT_GT(asprintf(&expect_raw, " %u %o %x %g %f %s %X %d %e %p", arg0, arg1, arg2, arg3,
						   arg4, arg5, arg6, arg7, arg8, arg9),
				  10);
		ASSERT_TRUE(expect_raw);
		expect = std::string(expect_raw);
		free(expect_raw);
	}

	auto f = fopen(m_file_name.c_str(), "r+");
	fseek(f, 0, SEEK_END);
	const size_t fileSize = (size_t)ftell(f);
	rewind(f);
	EXPECT_LT(fileSize, 1024);
	for (size_t offset = 0; offset < fileSize; offset++) {
		uint16_t old = 0;
		EXPECT_EQ(pread(fileno(f), &old, 1, (long)offset), 1) << strerror(errno);
		for (uint16_t damaged = 0; damaged <= UINT8_MAX; damaged++) {
			if (damaged == old) continue;
			EXPECT_EQ(pwrite(fileno(f), &damaged, 1, (long)offset), 1) << strerror(errno);
			fflush(f);
			auto tb = SyncTracebuffer::make(m_file_name);
			if (tb) {
				auto tp = tb->next();
				if (tp) (void)tp->msg();
			}
		}
		EXPECT_EQ(pwrite(fileno(f), &old, 1, (long)offset), 1) << strerror(errno);
	}
}