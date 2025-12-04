// Copyright (c) 2024, International Business Machines
// SPDX-License-Identifier: BSD-2-Clause-Patent

#ifndef CLLTK_CMD_LIVE_ORDERED_BUFFER_HPP
#define CLLTK_CMD_LIVE_ORDERED_BUFFER_HPP

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <optional>
#include <vector>

#include "CommonLowLevelTracingKit/decoder/Tracepoint.hpp"

namespace CommonLowLevelTracingKit::cmd::live
{

using TracepointPtr = decoder::TracepointPtr;

/**
 * @brief Thread-safe ordered buffer for live streaming tracepoints
 *
 * Design:
 * - Min-heap ordered by timestamp (oldest first)
 * - Watermark tracking: only releases tracepoints older than (watermark - delay)
 * - Configurable max size with oldest-drop policy when full
 * - Thread-safe: single reader thread pushes, single output thread pops
 *
 * The watermark represents the maximum timestamp we've seen from tracebuffers.
 * This allows us to safely output tracepoints that are older than
 * (watermark - delay_ns) because we know no older tracepoints will arrive.
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

	/**
	 * @brief Construct ordered buffer
	 * @param max_size Maximum number of tracepoints to buffer (0 = unlimited)
	 * @param order_delay_ns Time delay in nanoseconds for ordering safety
	 */
	explicit OrderedBuffer(size_t max_size = 10000,
						   uint64_t order_delay_ns = 100'000'000 /* 100ms */);

	~OrderedBuffer() = default;

	// Non-copyable, non-movable
	OrderedBuffer(const OrderedBuffer &) = delete;
	OrderedBuffer &operator=(const OrderedBuffer &) = delete;
	OrderedBuffer(OrderedBuffer &&) = delete;
	OrderedBuffer &operator=(OrderedBuffer &&) = delete;

	/**
	 * @brief Push a tracepoint into the buffer
	 *
	 * Thread-safe. If buffer is full, drops the oldest tracepoint.
	 *
	 * @param tp Tracepoint to push (takes ownership)
	 * @return true if pushed successfully
	 */
	bool push(TracepointPtr tp);

	/**
	 * @brief Update the watermark timestamp
	 *
	 * Called by reader thread to indicate the maximum timestamp seen.
	 * Tracepoints older than (watermark - delay) are safe to output.
	 *
	 * @param max_seen_ns Maximum timestamp seen across all tracebuffers
	 */
	void update_watermark(uint64_t max_seen_ns);

	/**
	 * @brief Signal that no more tracepoints will be pushed
	 *
	 * Called when reader thread is stopping. Allows remaining tracepoints
	 * to be flushed without waiting for watermark.
	 */
	void finish();

	/**
	 * @brief Pop the oldest tracepoint if it's safe to output
	 *
	 * A tracepoint is safe to output if:
	 * - Buffer is finished (flushing remaining), OR
	 * - Its timestamp is older than (watermark - order_delay)
	 *
	 * @param timeout Maximum time to wait for a ready tracepoint
	 * @return Tracepoint if available and safe, nullopt otherwise
	 */
	std::optional<TracepointPtr> pop(std::chrono::milliseconds timeout = std::chrono::milliseconds{
										 100});

	/**
	 * @brief Pop all ready tracepoints (non-blocking)
	 *
	 * Returns a vector of all tracepoints that are safe to output.
	 */
	std::vector<TracepointPtr> pop_all_ready();

	/**
	 * @brief Check if buffer is empty
	 */
	[[nodiscard]] bool empty() const;

	/**
	 * @brief Get current buffer size
	 */
	[[nodiscard]] size_t size() const;

	/**
	 * @brief Check if buffer is finished (no more pushes expected)
	 */
	[[nodiscard]] bool finished() const;

	/**
	 * @brief Get buffer statistics
	 */
	[[nodiscard]] Stats stats() const;

	/**
	 * @brief Get the order delay setting
	 */
	[[nodiscard]] uint64_t order_delay_ns() const noexcept;

	/**
	 * @brief Get the max size setting
	 */
	[[nodiscard]] size_t max_size() const noexcept;

  private:
	// Comparator for min-heap (oldest timestamp first)
	// Note: std::make_heap etc. create a max-heap by default, so we use
	// greater-than to get min-heap behavior (smallest/oldest at front)
	struct TimestampCompare {
		bool operator()(const TracepointPtr &a, const TracepointPtr &b) const;
	};

	/**
	 * @brief Pop front element from the heap (must hold lock)
	 * @return The oldest tracepoint
	 */
	TracepointPtr pop_front_locked();

	/**
	 * @brief Check if there's a ready tracepoint (must hold lock)
	 */
	bool has_ready_locked() const;

	const size_t m_max_size;
	const uint64_t m_order_delay_ns;

	mutable std::mutex m_mutex;
	std::condition_variable m_cv;
	std::vector<TracepointPtr> m_heap; // Min-heap via std::*_heap algorithms
	uint64_t m_watermark_ns{0};
	bool m_finished{false};
	Stats m_stats{};
};

} // namespace CommonLowLevelTracingKit::cmd::live

#endif // CLLTK_CMD_LIVE_ORDERED_BUFFER_HPP
