// Copyright (c) 2024, International Business Machines
// SPDX-License-Identifier: BSD-2-Clause-Patent

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <compare>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <latch>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <span>
#include <sstream>
#include <stdint.h>
#include <stdio.h>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#include "CommonLowLevelTracingKit/tracing/tracing.h"
#include "helper.hpp"
#include "ringbuffer.hpp"
#include "tracebufferfile.hpp"
#include "gtest/gtest.h"

#define TB CLLTK_TRACEBUFFER_MACRO_VALUE(decoder_ringbuffer)
constexpr size_t TB_SIZE = 512;
CLLTK_TRACEBUFFER(TB, TB_SIZE)

using namespace CommonLowLevelTracingKit;
using namespace CommonLowLevelTracingKit::decoder;
using namespace CommonLowLevelTracingKit::decoder::source;
using namespace std::string_literals;
using namespace std::chrono;
using EntryPtr = Ringbuffer::EntryPtr;
class decoder_ringbuffer : public ::testing::Test
{
  protected:
	std::string m_file_name;
	std::string m_tb_name;

	std::unique_lock<std::mutex> guard;
	void SetUp() override
	{
		m_tb_name = STR(TB);
		m_file_name = trace_file(m_tb_name);
		SETUP(TB);
	}
	void TearDown() override { CLEANUP(TB); }
};

TEST_F(decoder_ringbuffer, valid_header)
{
	TracebufferFile tb{m_file_name};
	[[maybe_unused]]
	Ringbuffer &rb = tb.getRingbuffer();
}

TEST_F(decoder_ringbuffer, empty)
{
	TracebufferFile tb{m_file_name};
	[[maybe_unused]]
	Ringbuffer &rb = tb.getRingbuffer();
	EXPECT_FALSE(get<0>(rb.getNextEntry()));
}
TEST_F(decoder_ringbuffer, get_one_entries)
{
	TracebufferFile tb{m_file_name};
	Ringbuffer &rb = tb.getRingbuffer();
	ASSERT_FALSE(get<0>(rb.getNextEntry()));
	TP("Hello World");
	EXPECT_TRUE(get<0>(rb.getNextEntry()));
	EXPECT_FALSE(get<0>(rb.getNextEntry()));
}
TEST_F(decoder_ringbuffer, get_two_entries)
{
	TracebufferFile tb{m_file_name};
	Ringbuffer &rb = tb.getRingbuffer();
	ASSERT_FALSE(get<0>(rb.getNextEntry()));
	TP("Hello World");
	TP("Hello World");
	EXPECT_TRUE(get<0>(rb.getNextEntry()));
	EXPECT_TRUE(get<0>(rb.getNextEntry()));
	EXPECT_FALSE(get<0>(rb.getNextEntry()));
}
TEST_F(decoder_ringbuffer, get_wrapped)
{
	Ringbuffer::EntryPtr e{nullptr};
	TracebufferFile tb{m_file_name};
	Ringbuffer &rb = tb.getRingbuffer();
	e = get<0>(rb.getNextEntry());
	ASSERT_FALSE(e);

	clltk_dynamic_tracepoint_execution(m_tb_name.c_str(), "", 0, 0, 0, "A");

	e = get<0>(rb.getNextEntry());
	ASSERT_TRUE(e);
	ASSERT_EQ(e->size(), 33);
	ASSERT_EQ(e->body()[31], 'A');

	clltk_dynamic_tracepoint_execution(m_tb_name.c_str(), "", 0, 0, 0, "B");
	e = get<0>(rb.getNextEntry());
	ASSERT_TRUE(e);
	ASSERT_EQ(e->size(), 33);
	ASSERT_EQ(e->body()[31], 'B');

	clltk_dynamic_tracepoint_execution(m_tb_name.c_str(), "", 0, 0, 0, "C");
	e = get<0>(rb.getNextEntry());
	ASSERT_TRUE(e);
	ASSERT_EQ(e->size(), 33);
	ASSERT_EQ(e->body()[31], 'C');
}
TEST_F(decoder_ringbuffer, get_entries)
{
	Ringbuffer::EntryPtr e{nullptr};
	clltk_dynamic_tracebuffer_creation(m_tb_name.c_str(), 33 + 16);
	TracebufferFile tb{m_file_name};
	Ringbuffer &rb = tb.getRingbuffer();
	e = get<0>(rb.getNextEntry());
	ASSERT_FALSE(e);
	for (char i = 0; i < 10; i++) {
		std::string msg = {(char)('A' + i), '\0'};
		clltk_dynamic_tracepoint_execution(m_tb_name.c_str(), "", 0, 0, 0, "%s", msg.c_str());
		e = get<0>(rb.getNextEntry());
		EXPECT_TRUE(e);
		ASSERT_EQ(e->size(), 33);
		ASSERT_EQ(e->body()[31], msg[0]) << "index = " << (int)i;
		ASSERT_FALSE(get<0>(rb.getNextEntry())) << "index = " << (int)i;
	}
}
TEST_F(decoder_ringbuffer, overtaken)
{
	clltk_dynamic_tracepoint_execution(m_tb_name.c_str(), __FILE__, __LINE__, 0, 0, "Hello World");
	TracebufferFile tb{m_file_name};
	Ringbuffer &rb = tb.getRingbuffer();
	EXPECT_TRUE(get<0>(rb.getNextEntry()));
	EXPECT_FALSE(get<0>(rb.getNextEntry()));
	for (size_t i = 0; i < 20; i++) {
		clltk_dynamic_tracepoint_execution(m_tb_name.c_str(), __FILE__, __LINE__, 0, 0,
										   "Hello World");
	}
	EXPECT_TRUE(get<0>(rb.getNextEntry()));
}

TEST_F(decoder_ringbuffer, recover_after_reset)
{
	const auto tp = []() { CLLTK_TRACEPOINT(TB, "a"); };
	const auto hash = [](auto &rb) {
		const auto entry = get<0>(rb.getNextEntry());
		const auto body = entry->body();
		const std::string_view sv{reinterpret_cast<const char *>(body.data()), body.size()};
		return std::hash<std::string_view>{}(sv);
	};
	std::set<size_t> know;
	TracebufferFile tb{m_file_name};
	Ringbuffer &rb = tb.getRingbuffer();
	ASSERT_FALSE(rb.pendingBytes());
	tp();
	ASSERT_TRUE(rb.pendingBytes());
	tp();
	ASSERT_TRUE(rb.pendingBytes());
	EXPECT_TRUE(know.insert(hash(rb)).second);
	rb.reset();
	EXPECT_TRUE(know.insert(hash(rb)).second);
	EXPECT_FALSE(get<0>(rb.getNextEntry()));
}
TEST_F(decoder_ringbuffer, recover_after_drop)
{
	const auto tp = []() { CLLTK_TRACEPOINT(TB, "a"); };
	const auto hash = std::hash<std::span<const uint8_t>>();
	std::set<size_t> know;
	TracebufferFile tb{m_file_name};
	Ringbuffer &rb = tb.getRingbuffer();
	ASSERT_FALSE(rb.getDropped());
	while (rb.pendingBytes() < rb.getSize() / 2) tp();
	while (rb.pendingBytes())
		ASSERT_TRUE(know.insert(hash(get<0>(rb.getNextEntry())->body())).second);
	while (!rb.getDropped()) tp();
	while (const auto r = rb.pendingBytes()) {
		std::cout << " pending " << r << std::endl;
		ASSERT_TRUE(know.insert(hash(get<0>(rb.getNextEntry())->body())).second);
	}
}

// might fail during debugging
TEST_F(decoder_ringbuffer, write_read_parallel_one_thread)
{
	std::atomic<bool> keep_running{true};
	std::latch l{2};
	const auto hash = std::hash<std::span<const uint8_t>>();
	const auto write_thread_func = [&]() {
		l.arrive_and_wait();
		while (keep_running.load(std::memory_order_relaxed)) { CLLTK_TRACEPOINT(TB, "A"); };
	};
	std::thread write_thread{write_thread_func};
	TracebufferFile tb{m_file_name};
	Ringbuffer &rb = tb.getRingbuffer();
	l.arrive_and_wait();
	std::set<size_t> known{};
	size_t r = 0;
	for (uint64_t read_index = 0; read_index < 100; read_index++) {
		auto lastMsg = steady_clock::now();
		while ((steady_clock::now() - lastMsg) < milliseconds(10)) {
			const auto e = get<0>(rb.getNextEntry());
			if (e == nullptr) continue;
			EXPECT_TRUE(known.insert(hash(e->body())).second);
			r++;
			lastMsg = steady_clock::now();
			break;
		}
	}
	keep_running.store(false, std::memory_order_relaxed);
	write_thread.join();
	while (rb.pendingBytes()) {
		const auto e = get<0>(rb.getNextEntry());
		if (!e) continue;
		EXPECT_TRUE(known.insert(hash(e->body())).second);
	}
	std::cout << " write = " << rb.getEntryCount() << std::endl;
	std::cout << " read  = " << r << std::endl;
}

// might fail during debugging
TEST_F(decoder_ringbuffer, write_read_parallel_overwhelmed)
{
	constexpr size_t n_threads = 100;
	constexpr size_t n_tp_per_thread = 1000;
	constexpr size_t n_tp_total = n_threads * n_tp_per_thread;
	const auto write_thread_func = [&]() {
		for (uint64_t write_index = 0; write_index < n_tp_per_thread; write_index++) {
			char buf[52] = {};
			sprintf(buf, "START:%lu", write_index++);
			CLLTK_TRACEPOINT(TB, "%s", buf);
			std::this_thread::sleep_for(microseconds(1));
		}
	};

	std::array<std::thread, n_threads> threads;
	std::for_each(threads.begin(), threads.end(),
				  [&](std::thread &t) { t = std::thread{write_thread_func}; });

	TracebufferFile tb{m_file_name};
	Ringbuffer &rb = tb.getRingbuffer();
	std::vector<uint64_t> known;
	known.reserve(n_tp_total);

	auto lastMsg = steady_clock::now();
	while ((steady_clock::now() - lastMsg) < milliseconds(100)) {
		auto rc = rb.getNextEntry();
		if (rc.index() != 0) continue;
		auto e = std::move(get<0>(rc));
		if (e == nullptr) continue;
		lastMsg = steady_clock::now();
		known.push_back(e->nr);
	}
	std::for_each(threads.begin(), threads.end(), [](std::thread &t) { t.join(); });
	EXPECT_FALSE(get<0>(rb.getNextEntry())) << "there should be no new tracepoints";

	std::set<uint64_t> set{};
	for (const uint64_t n : known) EXPECT_TRUE(set.insert(n).second) << "double = " << n;

	const double quota = static_cast<double>(known.size()) * 100 / n_tp_total;
	std::cout << "read = " << known.size() << "/" << n_tp_total << "(" << quota << ")" << std::endl;
	EXPECT_GT(quota, 5.0);
	EXPECT_LE(quota, 100.0);
}

TEST_F(decoder_ringbuffer, read_parallel_overwhelmed)
{
	TracebufferFile tb{m_file_name};
	Ringbuffer &rb = tb.getRingbuffer();

	constexpr size_t n_threads = 40;
	constexpr size_t n_tp_per_thread = 2500;
	constexpr size_t n_tp_total = n_threads * n_tp_per_thread;
	std::latch l{n_threads + 1};
	const auto func = [](size_t i = 0) { CLLTK_TRACEPOINT(TB, "%lu", i++); };
	const auto write_thread_func = [&l, &func](const size_t n_max) {
		l.arrive_and_wait();
		for (uint64_t i = 0; i < n_max; i++) {
			func(i);
			std::this_thread::sleep_for(microseconds(1));
		}
	};

	ASSERT_FALSE(rb.pendingBytes());
	std::array<std::thread, n_threads> threads;
	std::for_each(threads.begin(), threads.end(),
				  [&](std::thread &t) { t = std::thread{write_thread_func, n_tp_per_thread}; });

	std::vector<uint64_t> nrs_vector{};
	nrs_vector.reserve((n_tp_total * 12) / 10);
	l.arrive_and_wait();
	auto lastMsg = steady_clock::now();
	while ((steady_clock::now() - lastMsg) < milliseconds(100)) {
		if (auto rc = rb.getNextEntry(); rc.index() == 0) {
			const auto tp = std::move(get<0>(rc));
			if (tp == nullptr) continue;
			lastMsg = steady_clock::now();
			nrs_vector.push_back(tp->nr);
		};
	}
	std::for_each(threads.begin(), threads.end(), [](std::thread &t) { t.join(); });
	EXPECT_EQ(rb.pendingBytes(), 0) << "there should be no new tracepoints ";

	std::map<uint64_t, uint64_t> frequency;
	for (uint64_t num : nrs_vector) { frequency[num]++; }
	std::stringstream msg;
	for (const auto &entry : frequency) {
		if (entry.second > 1) { msg << entry.first << " x " << entry.second << std::endl; }
	}
	if (!msg.str().empty()) ADD_FAILURE() << msg.str();

	std::cout << "read = " << frequency.size() << "/" << n_tp_total << std::endl;
	std::cout << "read = " << nrs_vector.size() << "/" << n_tp_total << std::endl;
	EXPECT_LE(nrs_vector.size(), n_tp_total);
}

TEST_F(decoder_ringbuffer, get_pendingBytes)
{
	auto tb = TracebufferFile(m_file_name);
	auto &rb = tb.getRingbuffer();
	const auto tp = []() { CLLTK_TRACEPOINT(TB, "%s", "a"); };

	EXPECT_FALSE(rb.pendingBytes());
	tp();
	const auto tp_size = rb.pendingBytes();
	std::cout << "tp_size = " << tp_size << std::endl;
	EXPECT_TRUE(tp_size);
	ASSERT_TRUE(get<0>(rb.getNextEntry()));
	EXPECT_FALSE(rb.pendingBytes());
	tp();
	EXPECT_EQ(tp_size, rb.pendingBytes());
	ASSERT_TRUE(get<0>(rb.getNextEntry()));
	EXPECT_FALSE(rb.pendingBytes());
	ASSERT_FALSE(get<0>(rb.getNextEntry()));
	EXPECT_FALSE(rb.pendingBytes());

	const size_t max_tp_count = rb.getSize() / tp_size;
	std::cout << "rb.getSize() = " << rb.getSize() << std::endl;
	std::cout << "max_tp_count = " << max_tp_count << std::endl;
	for (uint32_t i = 1; i <= max_tp_count; i++) {
		tp();
		ASSERT_EQ(rb.pendingBytes(), i * tp_size) << i;
		ASSERT_TRUE(get<0>(rb.getNextEntry()));
		ASSERT_EQ(rb.pendingBytes(), (i - 1) * tp_size) << i;
		tp();
		ASSERT_EQ(rb.pendingBytes(), i * tp_size) << i;
	}

	ASSERT_EQ(rb.pendingBytes(), max_tp_count * tp_size);
	tp();
	for (uint32_t i = 1; i <= tp_size * max_tp_count * 10; i++) {
		ASSERT_EQ(rb.pendingBytes(), max_tp_count * tp_size);
		tp();
		ASSERT_EQ(rb.pendingBytes(), max_tp_count * tp_size) << i;
		EXPECT_TRUE(get<0>(rb.getNextEntry()));
		EXPECT_EQ(rb.pendingBytes(), (max_tp_count - 1) * tp_size) << i;
		tp();
	}
	ASSERT_EQ(rb.pendingBytes(), max_tp_count * tp_size);
}

TEST_F(decoder_ringbuffer, pendingBytes_always_full)
{
	auto tb = TracebufferFile(m_file_name);
	auto &rb = tb.getRingbuffer();
	static constexpr std::string_view sv{"||||||||||||||||||||||||||||||||||||||||||||"};
	const auto tp = [](size_t i = 0) {
		char buf[sv.size() + 1] = {};
		std::strncpy(buf, sv.cbegin(), std::min(i, sv.size()));
		CLLTK_TRACEPOINT(TB, "%s", buf);
	};

	for (size_t arg_size = 0; arg_size < sv.size(); arg_size++) {
		std::cout << "\narg_size = " << arg_size << std::endl;
		while (get<0>(rb.getNextEntry())); // clear ringbuffer
		EXPECT_FALSE(rb.pendingBytes());
		tp(arg_size);
		const auto tp_size = rb.pendingBytes();
		std::cout << "tp_size = " << tp_size << std::endl;
		EXPECT_TRUE(tp_size);
		EXPECT_TRUE(get<0>(rb.getNextEntry()));
		EXPECT_FALSE(rb.pendingBytes());
		while (rb.pendingBytes() <= (rb.getSize() - tp_size))
			(tp(arg_size)); // fill ringbuffer with current size

		const auto pending = rb.pendingBytes();
		ASSERT_GE(pending, rb.getSize() - tp_size);
		ASSERT_LE(pending, rb.getSize());
		std::cout << "rb.getSize() = " << rb.getSize() << std::endl;
		std::cout << "pending = " << pending << std::endl;

		const size_t max_tp_count = rb.getSize() / tp_size;
		std::cout << "max_tp_count = " << max_tp_count << std::endl;
		for (uint32_t i = 1; i <= max_tp_count * tp_size * 10; i++) {
			tp(arg_size);
			ASSERT_EQ(pending, rb.pendingBytes());
		}
	}
}
TEST_F(decoder_ringbuffer, pendingBytes_always_full_with_getNext)
{
	auto tb = TracebufferFile(m_file_name);
	auto &rb = tb.getRingbuffer();
	static constexpr std::string_view sv{"||||||||||||||||||||||||||||||||||||||||||||"};
	const auto tp = [](size_t i = 0) {
		char buf[sv.size() + 1] = {};
		std::strncpy(buf, sv.cbegin(), std::min(i, sv.size()));
		CLLTK_TRACEPOINT(TB, "%s", buf);
	};

	for (size_t arg_size = 0; arg_size < sv.size(); arg_size++) {
		std::cout << "\narg_size = " << arg_size << std::endl;
		while (get<0>(rb.getNextEntry())); // clear ringbuffer
		EXPECT_FALSE(rb.pendingBytes());
		tp(arg_size);
		const auto tp_size = rb.pendingBytes();
		std::cout << "tp_size = " << tp_size << std::endl;
		EXPECT_TRUE(tp_size);
		EXPECT_TRUE(get<0>(rb.getNextEntry()));
		EXPECT_FALSE(rb.pendingBytes());
		while (rb.pendingBytes() <= (rb.getSize() - tp_size))
			(tp(arg_size)); // fill ringbuffer with current size

		const auto pending = rb.pendingBytes();
		ASSERT_GE(pending, rb.getSize() - tp_size);
		ASSERT_LE(pending, rb.getSize());
		std::cout << "rb.getSize() = " << rb.getSize() << std::endl;
		std::cout << "pending = " << pending << std::endl;

		std::set<uint64_t> known{};

		const size_t max_tp_count = rb.getSize() / tp_size;
		std::cout << "max_tp_count = " << max_tp_count << std::endl;
		for (uint32_t i = 1; i <= max_tp_count * tp_size * 10; i++) {
			tp(arg_size);
			const auto e = get<0>(rb.getNextEntry());
			ASSERT_TRUE(known.insert(e->nr).second);
			tp(arg_size);
		}
	}
}