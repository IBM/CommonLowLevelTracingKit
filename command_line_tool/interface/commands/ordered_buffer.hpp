// Copyright (c) 2024, International Business Machines
// SPDX-License-Identifier: BSD-2-Clause-Patent

#ifndef CLLTK_CMD_LIVE_ORDERED_BUFFER_HPP
#define CLLTK_CMD_LIVE_ORDERED_BUFFER_HPP

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <optional>
#include <vector>

#include "CommonLowLevelTracingKit/decoder/Tracepoint.hpp"

namespace CommonLowLevelTracingKit::cmd::interface
{

using TracepointPtr = decoder::TracepointPtr;

/**
 * Thread-safe ordered buffer for live streaming tracepoints.
 * Min-heap by timestamp with watermark-based release. Drops oldest on overflow.
 */
class OrderedBuffer
{
  public:
	struct Stats {
		uint64_t total_pushed{0};
		uint64_t total_popped{0};
		uint64_t total_dropped{0};
		uint64_t current_size{0};
		uint64_t high_water_mark{0};
		uint64_t watermark_ns{0};
	};

	explicit OrderedBuffer(size_t max_size = 10000,
						   uint64_t order_delay_ns = 100'000'000 /* 100ms */);

	~OrderedBuffer() = default;

	// Non-copyable, non-movable
	OrderedBuffer(const OrderedBuffer &) = delete;
	OrderedBuffer &operator=(const OrderedBuffer &) = delete;
	OrderedBuffer(OrderedBuffer &&) = delete;
	OrderedBuffer &operator=(OrderedBuffer &&) = delete;

	// Push tracepoint (takes ownership). Drops oldest if full.
	bool push(TracepointPtr tp);

	void update_watermark(uint64_t max_seen_ns); // allows releasing old tracepoints

	void finish(); // signal end of input for flushing

	// Pop oldest if safe (ts < watermark - delay or finished)
	std::optional<TracepointPtr> pop(std::chrono::milliseconds timeout = std::chrono::milliseconds{
										 100});

	std::vector<TracepointPtr> pop_all_ready();

	[[nodiscard]] bool empty() const;
	[[nodiscard]] size_t size() const;
	[[nodiscard]] bool finished() const;
	[[nodiscard]] Stats stats() const;
	[[nodiscard]] uint64_t order_delay_ns() const noexcept;
	[[nodiscard]] size_t max_size() const noexcept;

  private:
	// Comparator for min-heap (oldest timestamp first)
	// Note: std::make_heap etc. create a max-heap by default, so we use
	// greater-than to get min-heap behavior (smallest/oldest at front)
	struct TimestampCompare {
		bool operator()(const TracepointPtr &a, const TracepointPtr &b) const;
	};

	TracepointPtr pop_front_locked(); // must hold lock
	bool has_ready_locked() const;	  // must hold lock

	const size_t m_max_size;
	const uint64_t m_order_delay_ns;

	mutable std::mutex m_mutex;
	std::condition_variable m_cv;
	std::vector<TracepointPtr> m_heap; // Min-heap via std::*_heap algorithms
	uint64_t m_watermark_ns{0};
	std::atomic<bool> m_finished{false}; // Atomic for lock-free read in output loop
	Stats m_stats{};
};

} // namespace CommonLowLevelTracingKit::cmd::interface

#endif // CLLTK_CMD_LIVE_ORDERED_BUFFER_HPP
