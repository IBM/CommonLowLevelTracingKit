// Copyright (c) 2024, International Business Machines
// SPDX-License-Identifier: BSD-2-Clause-Patent

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <random>
#include <thread>
#include <vector>

#include "commands/ordered_buffer.hpp"
#include "gtest/gtest.h"

using namespace CommonLowLevelTracingKit::cmd::interface;
using namespace CommonLowLevelTracingKit::decoder;
using namespace std::chrono_literals;

// =============================================================================
// Test Tracepoint for OrderedBuffer Testing
// =============================================================================

class TestTracepoint : public Tracepoint
{
  public:
	TestTracepoint(uint64_t ts, uint32_t p = 0, uint32_t t = 0)
		: Tracepoint(0, ts, SourceType::Userspace), m_pid(p), m_tid(t)
	{
	}

	Type type() const noexcept override { return Type::Dynamic; }
	const std::string_view tracebuffer() const noexcept override { return "test_buffer"; }
	const std::string_view file() const noexcept override { return "test.cpp"; }
	uint64_t line() const noexcept override { return 1; }
	uint32_t pid() const noexcept override { return m_pid; }
	uint32_t tid() const noexcept override { return m_tid; }
	const std::string_view msg() const override { return "test message"; }

  private:
	uint32_t m_pid;
	uint32_t m_tid;
};

// Helper to create TracepointPtr
inline TracepointPtr make_tracepoint(uint64_t ts, uint32_t pid = 0, uint32_t tid = 0)
{
	return TracepointPtr(new TestTracepoint(ts, pid, tid));
}

// =============================================================================
// Mock TracepointPtr for Testing
// =============================================================================

// The OrderedBuffer uses TracepointPtr which is a unique_ptr to Tracepoint.
// For testing, we need to create mock tracepoints. Since Tracepoint is complex,
// we'll test the buffer logic using the actual types from the decoder library.

// For these tests to compile, we need the decoder library linked.
// The tests focus on the buffer's ordering and thread-safety logic.

// =============================================================================
// Basic Construction Tests
// =============================================================================

class OrderedBufferConstructionTest : public ::testing::Test
{
};

TEST_F(OrderedBufferConstructionTest, DefaultConstruction)
{
	OrderedBuffer buffer;

	EXPECT_TRUE(buffer.empty());
	EXPECT_EQ(buffer.size(), 0UL);
	EXPECT_FALSE(buffer.finished());
	EXPECT_EQ(buffer.max_size(), 10000UL);
	EXPECT_EQ(buffer.order_delay_ns(), 100'000'000ULL); // 100ms default
}

TEST_F(OrderedBufferConstructionTest, CustomSizeAndDelay)
{
	OrderedBuffer buffer(5000, 50'000'000); // 5000 entries, 50ms delay

	EXPECT_EQ(buffer.max_size(), 5000UL);
	EXPECT_EQ(buffer.order_delay_ns(), 50'000'000ULL);
}

TEST_F(OrderedBufferConstructionTest, ZeroMaxSizeUnlimited)
{
	OrderedBuffer buffer(0, 100'000'000);

	EXPECT_EQ(buffer.max_size(), 0UL); // 0 means unlimited
}

// =============================================================================
// Stats Tests
// =============================================================================

class OrderedBufferStatsTest : public ::testing::Test
{
};

TEST_F(OrderedBufferStatsTest, InitialStats)
{
	OrderedBuffer buffer;
	auto stats = buffer.stats();

	EXPECT_EQ(stats.total_pushed, 0ULL);
	EXPECT_EQ(stats.total_popped, 0ULL);
	EXPECT_EQ(stats.total_dropped, 0ULL);
	EXPECT_EQ(stats.current_size, 0ULL);
	EXPECT_EQ(stats.high_water_mark, 0ULL);
	EXPECT_EQ(stats.watermark_ns, 0ULL);
}

// =============================================================================
// Watermark Tests
// =============================================================================

class OrderedBufferWatermarkTest : public ::testing::Test
{
};

TEST_F(OrderedBufferWatermarkTest, UpdateWatermark)
{
	OrderedBuffer buffer;
	buffer.update_watermark(1'000'000'000ULL); // 1 second

	auto stats = buffer.stats();
	EXPECT_EQ(stats.watermark_ns, 1'000'000'000ULL);
}

TEST_F(OrderedBufferWatermarkTest, WatermarkMonotonicallyIncreases)
{
	OrderedBuffer buffer;

	buffer.update_watermark(1'000'000'000ULL);
	auto stats1 = buffer.stats();
	EXPECT_EQ(stats1.watermark_ns, 1'000'000'000ULL);

	buffer.update_watermark(2'000'000'000ULL);
	auto stats2 = buffer.stats();
	EXPECT_EQ(stats2.watermark_ns, 2'000'000'000ULL);
}

// =============================================================================
// Finish Signal Tests
// =============================================================================

class OrderedBufferFinishTest : public ::testing::Test
{
};

TEST_F(OrderedBufferFinishTest, FinishEmptyBuffer)
{
	OrderedBuffer buffer;

	EXPECT_FALSE(buffer.finished());
	buffer.finish();
	EXPECT_TRUE(buffer.finished()); // Empty and finished
}

TEST_F(OrderedBufferFinishTest, FinishIdempotent)
{
	OrderedBuffer buffer;

	buffer.finish();
	EXPECT_TRUE(buffer.finished());

	buffer.finish(); // Should not crash
	EXPECT_TRUE(buffer.finished());
}

// =============================================================================
// Push Null Tests
// =============================================================================

class OrderedBufferPushNullTest : public ::testing::Test
{
};

TEST_F(OrderedBufferPushNullTest, PushNullReturnsfalse)
{
	OrderedBuffer buffer;

	bool result = buffer.push(nullptr);
	EXPECT_FALSE(result);
	EXPECT_TRUE(buffer.empty());
}

// =============================================================================
// Pop Ready Tests (without actual tracepoints)
// =============================================================================

class OrderedBufferPopReadyTest : public ::testing::Test
{
};

TEST_F(OrderedBufferPopReadyTest, PopAllReadyEmptyBuffer)
{
	OrderedBuffer buffer;

	auto ready = buffer.pop_all_ready();
	EXPECT_TRUE(ready.empty());
}

TEST_F(OrderedBufferPopReadyTest, PopTimeoutEmptyBuffer)
{
	OrderedBuffer buffer;

	auto result = buffer.pop(10ms);
	EXPECT_FALSE(result.has_value());
}

// =============================================================================
// Thread Safety Tests (structural)
// =============================================================================

class OrderedBufferThreadSafetyTest : public ::testing::Test
{
};

TEST_F(OrderedBufferThreadSafetyTest, ConcurrentEmptyAccess)
{
	OrderedBuffer buffer;
	std::atomic<bool> stop{false};
	std::atomic<int> empty_checks{0};
	std::atomic<int> size_checks{0};

	// Multiple threads checking empty/size concurrently
	std::vector<std::thread> threads;
	for (int i = 0; i < 4; ++i) {
		threads.emplace_back([&]() {
			while (!stop.load()) {
				(void)buffer.empty();
				++empty_checks;
				(void)buffer.size();
				++size_checks;
			}
		});
	}

	std::this_thread::sleep_for(50ms);
	stop.store(true);

	for (auto &t : threads) {
		t.join();
	}

	EXPECT_GT(empty_checks.load(), 0);
	EXPECT_GT(size_checks.load(), 0);
}

TEST_F(OrderedBufferThreadSafetyTest, ConcurrentStatsAccess)
{
	OrderedBuffer buffer;
	std::atomic<bool> stop{false};
	std::atomic<int> stats_checks{0};

	std::vector<std::thread> threads;
	for (int i = 0; i < 4; ++i) {
		threads.emplace_back([&]() {
			while (!stop.load()) {
				auto stats = buffer.stats();
				(void)stats; // Use stats to avoid optimization
				++stats_checks;
			}
		});
	}

	std::this_thread::sleep_for(50ms);
	stop.store(true);

	for (auto &t : threads) {
		t.join();
	}

	EXPECT_GT(stats_checks.load(), 0);
}

TEST_F(OrderedBufferThreadSafetyTest, ConcurrentWatermarkUpdate)
{
	OrderedBuffer buffer;
	std::atomic<bool> stop{false};
	std::atomic<int> updates{0};

	std::vector<std::thread> threads;
	for (size_t i = 0; i < 4; ++i) {
		threads.emplace_back([&, i]() {
			uint64_t ts = i * 1'000'000'000ULL;
			while (!stop.load()) {
				buffer.update_watermark(ts);
				ts += 1'000'000ULL;
				++updates;
			}
		});
	}

	std::this_thread::sleep_for(50ms);
	stop.store(true);

	for (auto &t : threads) {
		t.join();
	}

	EXPECT_GT(updates.load(), 0);

	// Watermark should be set to some value
	auto stats = buffer.stats();
	EXPECT_GT(stats.watermark_ns, 0ULL);
}

// =============================================================================
// Configuration Tests
// =============================================================================

class OrderedBufferConfigTest : public ::testing::Test
{
};

TEST_F(OrderedBufferConfigTest, SmallBuffer)
{
	OrderedBuffer buffer(10, 10'000'000); // 10 entries, 10ms

	EXPECT_EQ(buffer.max_size(), 10UL);
	EXPECT_EQ(buffer.order_delay_ns(), 10'000'000ULL);
}

TEST_F(OrderedBufferConfigTest, LargeBuffer)
{
	OrderedBuffer buffer(1'000'000, 1'000'000'000); // 1M entries, 1s

	EXPECT_EQ(buffer.max_size(), 1'000'000UL);
	EXPECT_EQ(buffer.order_delay_ns(), 1'000'000'000ULL);
}

TEST_F(OrderedBufferConfigTest, ZeroDelay)
{
	OrderedBuffer buffer(100, 0); // No ordering delay

	EXPECT_EQ(buffer.order_delay_ns(), 0ULL);
}

// =============================================================================
// Edge Cases
// =============================================================================

class OrderedBufferEdgeCaseTest : public ::testing::Test
{
};

TEST_F(OrderedBufferEdgeCaseTest, FinishThenPopAllReady)
{
	OrderedBuffer buffer;

	buffer.finish();
	auto ready = buffer.pop_all_ready();

	EXPECT_TRUE(ready.empty());
	EXPECT_TRUE(buffer.finished());
}

TEST_F(OrderedBufferEdgeCaseTest, MultipleFinishCalls)
{
	OrderedBuffer buffer;

	for (int i = 0; i < 10; ++i) {
		buffer.finish();
	}

	EXPECT_TRUE(buffer.finished());
}

TEST_F(OrderedBufferEdgeCaseTest, WatermarkZero)
{
	OrderedBuffer buffer;
	buffer.update_watermark(0);

	auto stats = buffer.stats();
	EXPECT_EQ(stats.watermark_ns, 0ULL);
}

TEST_F(OrderedBufferEdgeCaseTest, WatermarkMaxValue)
{
	OrderedBuffer buffer;
	buffer.update_watermark(UINT64_MAX);

	auto stats = buffer.stats();
	EXPECT_EQ(stats.watermark_ns, UINT64_MAX);
}

// =============================================================================
// Tests with Real TracepointPtr Data
// =============================================================================

class OrderedBufferWithDataTest : public ::testing::Test
{
  protected:
	// Helper to create TracepointPtr
	TracepointPtr make_tp(uint64_t ts) { return make_tracepoint(ts); }
};

TEST_F(OrderedBufferWithDataTest, PushSingleTracepoint)
{
	OrderedBuffer buffer;

	bool result = buffer.push(make_tp(1000));

	EXPECT_TRUE(result);
	EXPECT_FALSE(buffer.empty());
	EXPECT_EQ(buffer.size(), 1UL);

	auto stats = buffer.stats();
	EXPECT_EQ(stats.total_pushed, 1ULL);
	EXPECT_EQ(stats.current_size, 1ULL);
}

TEST_F(OrderedBufferWithDataTest, PushMultipleTracepoints)
{
	OrderedBuffer buffer;

	for (uint64_t i = 0; i < 10; ++i) {
		EXPECT_TRUE(buffer.push(make_tp(i * 1000)));
	}

	EXPECT_EQ(buffer.size(), 10UL);

	auto stats = buffer.stats();
	EXPECT_EQ(stats.total_pushed, 10ULL);
	EXPECT_EQ(stats.high_water_mark, 10ULL);
}

TEST_F(OrderedBufferWithDataTest, StatsUpdateCorrectly)
{
	OrderedBuffer buffer;

	buffer.push(make_tp(1000));
	buffer.push(make_tp(2000));
	buffer.push(make_tp(3000));

	auto stats = buffer.stats();
	EXPECT_EQ(stats.total_pushed, 3ULL);
	EXPECT_EQ(stats.current_size, 3ULL);
	EXPECT_EQ(stats.high_water_mark, 3ULL);
	EXPECT_EQ(stats.total_dropped, 0ULL);
}

// =============================================================================
// Ordering Tests - Verify Min-Heap Produces Oldest First
// =============================================================================

class OrderedBufferOrderingTest : public ::testing::Test
{
  protected:
	TracepointPtr make_tp(uint64_t ts) { return make_tracepoint(ts); }
};

TEST_F(OrderedBufferOrderingTest, OrderedOutputAscending)
{
	OrderedBuffer buffer(100, 0); // No delay for easier testing

	// Push in random order
	buffer.push(make_tp(3000));
	buffer.push(make_tp(1000));
	buffer.push(make_tp(2000));
	buffer.push(make_tp(5000));
	buffer.push(make_tp(4000));

	// Set watermark high enough to release all
	buffer.update_watermark(10000);

	auto ready = buffer.pop_all_ready();

	ASSERT_EQ(ready.size(), 5UL);
	EXPECT_EQ(ready[0]->timestamp_ns, 1000ULL);
	EXPECT_EQ(ready[1]->timestamp_ns, 2000ULL);
	EXPECT_EQ(ready[2]->timestamp_ns, 3000ULL);
	EXPECT_EQ(ready[3]->timestamp_ns, 4000ULL);
	EXPECT_EQ(ready[4]->timestamp_ns, 5000ULL);
}

TEST_F(OrderedBufferOrderingTest, OrderedOutputReverseInput)
{
	OrderedBuffer buffer(100, 0);

	// Push in reverse order
	for (uint64_t i = 10; i >= 1; --i) {
		buffer.push(make_tp(i * 1000));
	}

	buffer.update_watermark(100000);
	auto ready = buffer.pop_all_ready();

	ASSERT_EQ(ready.size(), 10UL);
	for (size_t i = 0; i < ready.size(); ++i) {
		EXPECT_EQ(ready[i]->timestamp_ns, (i + 1) * 1000ULL);
	}
}

TEST_F(OrderedBufferOrderingTest, OrderedOutputRandomInput)
{
	OrderedBuffer buffer(100, 0);

	// Generate random timestamps
	std::vector<uint64_t> timestamps;
	for (uint64_t i = 0; i < 20; ++i) {
		timestamps.push_back(i * 100 + 1000);
	}

	// Shuffle timestamps
	std::random_device rd;
	std::mt19937 g(rd());
	std::shuffle(timestamps.begin(), timestamps.end(), g);

	// Push in shuffled order
	for (uint64_t ts : timestamps) {
		buffer.push(make_tp(ts));
	}

	buffer.update_watermark(100000);
	auto ready = buffer.pop_all_ready();

	ASSERT_EQ(ready.size(), 20UL);

	// Verify output is sorted
	for (size_t i = 1; i < ready.size(); ++i) {
		EXPECT_LE(ready[i - 1]->timestamp_ns, ready[i]->timestamp_ns);
	}
}

TEST_F(OrderedBufferOrderingTest, DuplicateTimestamps)
{
	OrderedBuffer buffer(100, 0);

	// Push tracepoints with same timestamp
	buffer.push(make_tp(1000));
	buffer.push(make_tp(1000));
	buffer.push(make_tp(1000));

	buffer.update_watermark(10000);
	auto ready = buffer.pop_all_ready();

	ASSERT_EQ(ready.size(), 3UL);
	// All should have the same timestamp
	for (const auto &tp : ready) {
		EXPECT_EQ(tp->timestamp_ns, 1000ULL);
	}
}

// =============================================================================
// Drop Policy Tests - Verify Oldest-Drop When Full
// =============================================================================

class OrderedBufferDropPolicyTest : public ::testing::Test
{
  protected:
	TracepointPtr make_tp(uint64_t ts) { return make_tracepoint(ts); }
};

TEST_F(OrderedBufferDropPolicyTest, DropOldestWhenFull)
{
	OrderedBuffer buffer(3, 0); // Very small buffer

	// Push 3 tracepoints - buffer is now full
	buffer.push(make_tp(1000)); // oldest
	buffer.push(make_tp(2000));
	buffer.push(make_tp(3000));

	auto stats1 = buffer.stats();
	EXPECT_EQ(stats1.total_dropped, 0ULL);
	EXPECT_EQ(stats1.current_size, 3ULL);

	// Push one more - should drop the oldest (1000)
	buffer.push(make_tp(4000));

	auto stats2 = buffer.stats();
	EXPECT_EQ(stats2.total_dropped, 1ULL);
	EXPECT_EQ(stats2.current_size, 3ULL);
	EXPECT_EQ(stats2.total_pushed, 4ULL);

	// Verify oldest was dropped
	buffer.update_watermark(100000);
	auto ready = buffer.pop_all_ready();

	ASSERT_EQ(ready.size(), 3UL);
	EXPECT_EQ(ready[0]->timestamp_ns, 2000ULL); // 1000 was dropped
	EXPECT_EQ(ready[1]->timestamp_ns, 3000ULL);
	EXPECT_EQ(ready[2]->timestamp_ns, 4000ULL);
}

TEST_F(OrderedBufferDropPolicyTest, MultipleDrop)
{
	OrderedBuffer buffer(2, 0); // Very small buffer

	// Push more than capacity
	buffer.push(make_tp(1000));
	buffer.push(make_tp(2000));
	buffer.push(make_tp(3000)); // Drops 1000
	buffer.push(make_tp(4000)); // Drops 2000
	buffer.push(make_tp(5000)); // Drops 3000

	auto stats = buffer.stats();
	EXPECT_EQ(stats.total_dropped, 3ULL);
	EXPECT_EQ(stats.total_pushed, 5ULL);
	EXPECT_EQ(stats.current_size, 2ULL);

	buffer.update_watermark(100000);
	auto ready = buffer.pop_all_ready();

	ASSERT_EQ(ready.size(), 2UL);
	EXPECT_EQ(ready[0]->timestamp_ns, 4000ULL);
	EXPECT_EQ(ready[1]->timestamp_ns, 5000ULL);
}

TEST_F(OrderedBufferDropPolicyTest, UnlimitedBufferNoDrop)
{
	OrderedBuffer buffer(0, 0); // Unlimited

	for (uint64_t i = 0; i < 100; ++i) {
		buffer.push(make_tp(i * 1000));
	}

	auto stats = buffer.stats();
	EXPECT_EQ(stats.total_dropped, 0ULL);
	EXPECT_EQ(stats.total_pushed, 100ULL);
	EXPECT_EQ(stats.current_size, 100ULL);
}

TEST_F(OrderedBufferDropPolicyTest, HighWaterMarkTracking)
{
	OrderedBuffer buffer(5, 0);

	// Fill buffer
	for (uint64_t i = 0; i < 5; ++i) {
		buffer.push(make_tp(i * 1000));
	}

	auto stats1 = buffer.stats();
	EXPECT_EQ(stats1.high_water_mark, 5ULL);

	// Pop some
	buffer.update_watermark(100000);
	buffer.pop_all_ready();

	// High water mark should still be 5
	auto stats2 = buffer.stats();
	EXPECT_EQ(stats2.high_water_mark, 5ULL);
	EXPECT_EQ(stats2.current_size, 0ULL);
}

// =============================================================================
// Watermark-Based Readiness Tests
// =============================================================================

class OrderedBufferWatermarkReadinessTest : public ::testing::Test
{
  protected:
	TracepointPtr make_tp(uint64_t ts) { return make_tracepoint(ts); }
};

TEST_F(OrderedBufferWatermarkReadinessTest, NotReadyWithoutWatermark)
{
	OrderedBuffer buffer(100, 100'000'000); // 100ms delay

	buffer.push(make_tp(1'000'000'000)); // 1 second timestamp

	// Watermark is 0, so nothing is ready
	auto ready = buffer.pop_all_ready();
	EXPECT_TRUE(ready.empty());
	EXPECT_EQ(buffer.size(), 1UL);
}

TEST_F(OrderedBufferWatermarkReadinessTest, ReadyWhenWatermarkAdvances)
{
	OrderedBuffer buffer(100, 100'000'000); // 100ms delay

	buffer.push(make_tp(1'000'000'000)); // 1s
	buffer.push(make_tp(1'050'000'000)); // 1.05s
	buffer.push(make_tp(1'200'000'000)); // 1.2s

	// Set watermark to 1.15s - only 1s tracepoint should be ready
	// (1.15s - 0.1s = 1.05s, so 1s is <= 1.05s, but 1.05s is not < 1.05s)
	buffer.update_watermark(1'150'000'000);

	auto ready = buffer.pop_all_ready();
	ASSERT_EQ(ready.size(), 2UL); // 1.0s and 1.05s are ready
	EXPECT_EQ(ready[0]->timestamp_ns, 1'000'000'000ULL);
	EXPECT_EQ(ready[1]->timestamp_ns, 1'050'000'000ULL);
}

TEST_F(OrderedBufferWatermarkReadinessTest, AllReadyWhenWatermarkHigh)
{
	OrderedBuffer buffer(100, 100'000'000); // 100ms delay

	buffer.push(make_tp(1'000'000'000));
	buffer.push(make_tp(2'000'000'000));
	buffer.push(make_tp(3'000'000'000));

	// Set watermark way ahead
	buffer.update_watermark(10'000'000'000);

	auto ready = buffer.pop_all_ready();
	ASSERT_EQ(ready.size(), 3UL);
}

TEST_F(OrderedBufferWatermarkReadinessTest, AllReadyWhenFinished)
{
	OrderedBuffer buffer(100, 100'000'000); // 100ms delay

	buffer.push(make_tp(1'000'000'000));
	buffer.push(make_tp(2'000'000'000));
	buffer.push(make_tp(3'000'000'000));

	// Don't update watermark, just finish
	buffer.finish();

	auto ready = buffer.pop_all_ready();
	ASSERT_EQ(ready.size(), 3UL); // All flushed due to finish
}

TEST_F(OrderedBufferWatermarkReadinessTest, ZeroDelayImmediateReady)
{
	OrderedBuffer buffer(100, 0); // No delay

	buffer.push(make_tp(1000));

	// Any non-zero watermark makes it ready
	buffer.update_watermark(1000);

	auto ready = buffer.pop_all_ready();
	ASSERT_EQ(ready.size(), 1UL);
}

TEST_F(OrderedBufferWatermarkReadinessTest, WatermarkBelowDelay)
{
	OrderedBuffer buffer(100, 100'000'000); // 100ms delay

	buffer.push(make_tp(50'000'000)); // 50ms

	// Watermark is 90ms, delay is 100ms
	// safe_threshold = max(0, 90ms - 100ms) = 0
	// 50ms > 0, so not ready
	buffer.update_watermark(90'000'000);

	auto ready = buffer.pop_all_ready();
	EXPECT_TRUE(ready.empty());
}

TEST_F(OrderedBufferWatermarkReadinessTest, SafeThresholdCalculation)
{
	OrderedBuffer buffer(100, 50'000'000); // 50ms delay

	buffer.push(make_tp(100'000'000)); // 100ms
	buffer.push(make_tp(120'000'000)); // 120ms
	buffer.push(make_tp(200'000'000)); // 200ms

	// Watermark = 160ms, delay = 50ms
	// safe_threshold = 160ms - 50ms = 110ms
	// 100ms <= 110ms: ready
	// 120ms > 110ms: not ready
	buffer.update_watermark(160'000'000);

	auto ready = buffer.pop_all_ready();
	ASSERT_EQ(ready.size(), 1UL);
	EXPECT_EQ(ready[0]->timestamp_ns, 100'000'000ULL);
}

// =============================================================================
// Pop with Timeout Tests
// =============================================================================

class OrderedBufferPopTimeoutTest : public ::testing::Test
{
  protected:
	TracepointPtr make_tp(uint64_t ts) { return make_tracepoint(ts); }
};

TEST_F(OrderedBufferPopTimeoutTest, PopReturnsImmediatelyWhenReady)
{
	OrderedBuffer buffer(100, 0);

	buffer.push(make_tp(1000));
	buffer.update_watermark(10000);

	auto start = std::chrono::steady_clock::now();
	auto result = buffer.pop(1000ms);
	auto duration = std::chrono::steady_clock::now() - start;

	EXPECT_TRUE(result.has_value());
	EXPECT_LT(duration, 100ms); // Should return quickly
}

TEST_F(OrderedBufferPopTimeoutTest, PopTimesOutWhenNotReady)
{
	OrderedBuffer buffer(100, 100'000'000); // 100ms delay

	buffer.push(make_tp(1000));
	// Don't update watermark - nothing ready

	auto start = std::chrono::steady_clock::now();
	auto result = buffer.pop(50ms);
	auto duration = std::chrono::steady_clock::now() - start;

	EXPECT_FALSE(result.has_value());
	EXPECT_GE(duration, 45ms); // Should have waited approximately 50ms
}

TEST_F(OrderedBufferPopTimeoutTest, PopWakesOnFinish)
{
	OrderedBuffer buffer(100, 100'000'000);

	buffer.push(make_tp(1000));

	std::thread finisher([&]() {
		std::this_thread::sleep_for(50ms);
		buffer.finish();
	});

	auto start = std::chrono::steady_clock::now();
	auto result = buffer.pop(1000ms);
	auto duration = std::chrono::steady_clock::now() - start;

	finisher.join();

	EXPECT_TRUE(result.has_value()); // Got result due to finish
	EXPECT_LT(duration, 200ms);		 // Didn't wait full timeout
}

// =============================================================================
// Producer-Consumer Integration Tests
// =============================================================================

class OrderedBufferProducerConsumerTest : public ::testing::Test
{
  protected:
	TracepointPtr make_tp(uint64_t ts) { return make_tracepoint(ts); }
};

TEST_F(OrderedBufferProducerConsumerTest, SingleProducerSingleConsumer)
{
	OrderedBuffer buffer(100, 0);
	std::atomic<bool> done{false};
	std::vector<uint64_t> consumed_timestamps;
	std::mutex consumed_mutex;

	// Consumer thread
	std::thread consumer([&]() {
		while (!done.load() || !buffer.empty()) {
			auto result = buffer.pop(10ms);
			if (result.has_value()) {
				std::lock_guard lock(consumed_mutex);
				consumed_timestamps.push_back((*result)->timestamp_ns);
			}
		}
	});

	// Producer: push and update watermark
	for (uint64_t i = 0; i < 50; ++i) {
		buffer.push(make_tp(i * 1000));
		buffer.update_watermark((i + 1) * 1000);
		std::this_thread::sleep_for(1ms);
	}

	done.store(true);
	buffer.finish();
	consumer.join();

	// Verify all consumed
	EXPECT_EQ(consumed_timestamps.size(), 50UL);

	// Verify order
	for (size_t i = 1; i < consumed_timestamps.size(); ++i) {
		EXPECT_LE(consumed_timestamps[i - 1], consumed_timestamps[i]);
	}
}

TEST_F(OrderedBufferProducerConsumerTest, MultipleProducers)
{
	OrderedBuffer buffer(1000, 0);
	std::atomic<int> total_pushed{0};
	const uint64_t items_per_producer = 50;
	const int num_producers = 4;

	std::vector<std::thread> producers;
	for (uint64_t p = 0; p < static_cast<uint64_t>(num_producers); ++p) {
		producers.emplace_back([&, p]() {
			for (uint64_t i = 0; i < items_per_producer; ++i) {
				uint64_t ts = (p * items_per_producer + i) * 1000;
				buffer.push(make_tp(ts));
				buffer.update_watermark(ts + 1000);
				++total_pushed;
			}
		});
	}

	for (auto &t : producers) {
		t.join();
	}

	EXPECT_EQ(total_pushed.load(), num_producers * items_per_producer);

	buffer.update_watermark(UINT64_MAX);
	auto ready = buffer.pop_all_ready();

	EXPECT_EQ(ready.size(), static_cast<size_t>(num_producers * items_per_producer));

	// Verify ordering
	for (size_t i = 1; i < ready.size(); ++i) {
		EXPECT_LE(ready[i - 1]->timestamp_ns, ready[i]->timestamp_ns);
	}
}

// =============================================================================
// Finished State Tests
// =============================================================================

class OrderedBufferFinishedStateTest : public ::testing::Test
{
  protected:
	TracepointPtr make_tp(uint64_t ts) { return make_tracepoint(ts); }
};

TEST_F(OrderedBufferFinishedStateTest, FinishedOnlyWhenEmptyAndSignaled)
{
	OrderedBuffer buffer;

	buffer.push(make_tp(1000));

	// Signal finish but buffer not empty
	buffer.finish();
	EXPECT_FALSE(buffer.finished()); // Not finished because not empty

	// Drain buffer
	buffer.update_watermark(UINT64_MAX);
	buffer.pop_all_ready();

	EXPECT_TRUE(buffer.finished()); // Now finished
}

TEST_F(OrderedBufferFinishedStateTest, PushAfterFinishStillWorks)
{
	OrderedBuffer buffer;

	buffer.finish();
	EXPECT_TRUE(buffer.finished());

	// Push after finish - should still work (buffer implementation allows this)
	bool result = buffer.push(make_tp(1000));
	EXPECT_TRUE(result);
	EXPECT_FALSE(buffer.finished()); // No longer finished (has data)
}
