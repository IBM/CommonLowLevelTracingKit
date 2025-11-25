// Copyright (c) 2024, International Business Machines
// SPDX-License-Identifier: BSD-2-Clause-Patent

#include <algorithm>
#include <array>
#include <chrono>
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

#include "CommonLowLevelTracingKit/decoder/Tracebuffer.hpp"
#include "CommonLowLevelTracingKit/decoder/Tracepoint.hpp"
#include "CommonLowLevelTracingKit/tracing/tracing.h"
#include "helper.hpp"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
using ::testing::MatchesRegex;

#define TB CLLTK_TRACEBUFFER_MACRO_VALUE(decoder_TracepointStatic)
constexpr size_t TB_SIZE = 1024;
CLLTK_TRACEBUFFER(TB, TB_SIZE)

using namespace CommonLowLevelTracingKit::decoder;
using CommonLowLevelTracingKit::decoder::Tracepoint;
using namespace CommonLowLevelTracingKit::decoder::exception;
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
	std::ignore = tp->msg();
}

TEST_F(decoder_TracepointStatic, read_parallel_overwhelmed)
{
	auto tb = SyncTracebuffer::make(m_file_name);
	ASSERT_FALSE(tb->next()) << "should be empty";

	constexpr size_t n_threads = 10;
	constexpr size_t n_tp_per_thread = 2500;
	constexpr size_t n_tp_total = n_threads * n_tp_per_thread;
	std::latch l{n_threads + 1};
	const auto write_thread_func = [&l](const size_t th_nr, const size_t n_max) {
		l.arrive_and_wait();
		for (uint64_t i = 0; i < n_max; i++) {
			CLLTK_TRACEPOINT(TB, "%lu-%lu", th_nr, i);
			std::this_thread::sleep_for(microseconds(1));
		}
	};

	// start threads
	std::array<std::thread, n_threads> threads;
	for (size_t x = 0; x < threads.size(); x++)
		threads[x] = std::thread{write_thread_func, x, n_tp_per_thread};

	std::vector<std::string> timestamps{};
	timestamps.reserve((n_tp_total * 12) / 10);
	l.arrive_and_wait(); // start everything at the same time

	auto lastMsg = steady_clock::now();
	while ((steady_clock::now() - lastMsg) < milliseconds(100)) {
		const auto tp = tb->next();
		if (tp == nullptr) continue;
		if (tp->type() == Tracepoint::Type::Error) { continue; }
		lastMsg = steady_clock::now();
		try {
			const auto &msg = tp->msg();
			timestamps.push_back(std::string{msg});
		} catch (const std::exception &e) {
			ADD_FAILURE() << e.what();
		}
	}
	std::for_each(threads.begin(), threads.end(), [](std::thread &t) { t.join(); });
	EXPECT_FALSE(tb->next()) << "there should be no new tracepoints";

	std::map<std::string, uint64_t> frequency;
	for (const auto &num : timestamps) { frequency[num]++; }
	std::stringstream msg;
	size_t duplicates = 0;
	for (const auto &entry : frequency) {
		if (entry.second > 1) {
			duplicates += 1;
			msg << entry.first << "ns got " << entry.second << std::endl;
		}
	}
	if (!msg.str().empty()) ADD_FAILURE() << msg.str();

	std::cout << "" << frequency.size() + duplicates << "/" << timestamps.size() << std::endl;
	std::cout << "read total  = " << timestamps.size() << "/" << n_tp_total << std::endl;
	std::cout << "read unique = " << frequency.size() << "/" << n_tp_total << std::endl;
	EXPECT_LE(timestamps.size(), n_tp_total);
	std::cout << tb->path() << std::endl;

	const double quota = static_cast<double>(timestamps.size()) * 100 / n_tp_total;
	std::cout << "read = " << timestamps.size() << "/" << n_tp_total << "(" << quota << ")"
			  << std::endl;
	EXPECT_GT(quota, 10.0);
	EXPECT_LE(quota, 100.0);
}
