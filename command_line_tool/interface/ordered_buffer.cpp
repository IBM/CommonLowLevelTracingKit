// Copyright (c) 2024, International Business Machines
// SPDX-License-Identifier: BSD-2-Clause-Patent

#include "commands/ordered_buffer.hpp"

#include <algorithm>

namespace CommonLowLevelTracingKit::cmd::interface
{

OrderedBuffer::OrderedBuffer(size_t max_size, uint64_t order_delay_ns)
	: m_max_size(max_size), m_order_delay_ns(order_delay_ns)
{
	// Pre-allocate to reduce reallocations during operation
	if (max_size > 0) {
		m_heap.reserve(max_size);
	} else {
		m_heap.reserve(100000); // Reasonable default for unlimited mode
	}
}

bool OrderedBuffer::push(TracepointPtr tp)
{
	if (!tp)
		return false;

	std::lock_guard lock(m_mutex);

	if (m_max_size > 0 && m_heap.size() >= m_max_size) {
		// Drop the oldest tracepoint (smallest timestamp) to make room for new one.
		// Since TimestampCompare uses > for min-heap, the front element is the oldest.
		// pop_front_locked() removes and returns the front element.
		pop_front_locked();
		++m_stats.total_dropped;
	}

	m_heap.push_back(std::move(tp));
	std::push_heap(m_heap.begin(), m_heap.end(), TimestampCompare{});
	++m_stats.total_pushed;
	m_stats.current_size = m_heap.size();
	if (m_heap.size() > m_stats.high_water_mark) {
		m_stats.high_water_mark = m_heap.size();
	}

	// Note: We don't notify on every push - the output loop polls periodically
	// and the watermark update will notify when tracepoints become ready.
	// This reduces lock contention significantly.

	return true;
}

void OrderedBuffer::update_watermark(uint64_t max_seen_ns)
{
	std::lock_guard lock(m_mutex);
	m_watermark_ns = max_seen_ns;
	m_cv.notify_one();
}

void OrderedBuffer::finish()
{
	m_finished.store(true, std::memory_order_release);
	std::lock_guard lock(m_mutex);
	m_cv.notify_all();
}

std::optional<TracepointPtr> OrderedBuffer::pop(std::chrono::milliseconds timeout)
{
	std::unique_lock lock(m_mutex);

	// Wait for a tracepoint to be ready or finish signal
	if (!m_cv.wait_for(lock, timeout, [this] {
			return has_ready_locked() || m_finished.load(std::memory_order_acquire);
		})) {
		return std::nullopt; // Timeout
	}

	if (m_heap.empty()) {
		return std::nullopt;
	}

	if (m_finished.load(std::memory_order_acquire)) {
		// Flushing mode - output everything
		return pop_front_locked();
	}

	const uint64_t oldest_ts = m_heap.front()->timestamp_ns;
	const uint64_t safe_threshold =
		(m_watermark_ns > m_order_delay_ns) ? (m_watermark_ns - m_order_delay_ns) : 0;

	if (oldest_ts <= safe_threshold) {
		return pop_front_locked();
	}

	return std::nullopt;
}

std::vector<TracepointPtr> OrderedBuffer::pop_all_ready()
{
	std::vector<TracepointPtr> result;
	std::lock_guard lock(m_mutex);

	const bool is_finished = m_finished.load(std::memory_order_acquire);
	const uint64_t safe_threshold =
		(m_watermark_ns > m_order_delay_ns) ? (m_watermark_ns - m_order_delay_ns) : 0;

	while (!m_heap.empty()) {
		const uint64_t oldest_ts = m_heap.front()->timestamp_ns;

		if (is_finished || oldest_ts <= safe_threshold) {
			result.push_back(pop_front_locked());
		} else {
			break;
		}
	}

	m_stats.current_size = m_heap.size();
	return result;
}

bool OrderedBuffer::empty() const
{
	std::lock_guard lock(m_mutex);
	return m_heap.empty();
}

size_t OrderedBuffer::size() const
{
	std::lock_guard lock(m_mutex);
	return m_heap.size();
}

bool OrderedBuffer::finished() const
{
	// Fast path: check atomic finished flag without lock
	if (!m_finished.load(std::memory_order_acquire)) {
		return false;
	}
	// Need lock to check if heap is empty
	std::lock_guard lock(m_mutex);
	return m_heap.empty();
}

OrderedBuffer::Stats OrderedBuffer::stats() const
{
	std::lock_guard lock(m_mutex);
	Stats s = m_stats;
	s.current_size = m_heap.size();
	s.watermark_ns = m_watermark_ns;
	return s;
}

uint64_t OrderedBuffer::order_delay_ns() const noexcept
{
	return m_order_delay_ns;
}

size_t OrderedBuffer::max_size() const noexcept
{
	return m_max_size;
}

bool OrderedBuffer::TimestampCompare::operator()(const TracepointPtr &a,
												 const TracepointPtr &b) const
{
	return a->timestamp_ns > b->timestamp_ns;
}

TracepointPtr OrderedBuffer::pop_front_locked()
{
	std::pop_heap(m_heap.begin(), m_heap.end(), TimestampCompare{});
	TracepointPtr tp = std::move(m_heap.back());
	m_heap.pop_back();
	++m_stats.total_popped;
	m_stats.current_size = m_heap.size();
	return tp;
}

bool OrderedBuffer::has_ready_locked() const
{
	if (m_heap.empty())
		return false;
	if (m_finished.load(std::memory_order_acquire))
		return true;

	const uint64_t oldest_ts = m_heap.front()->timestamp_ns;
	const uint64_t safe_threshold =
		(m_watermark_ns > m_order_delay_ns) ? (m_watermark_ns - m_order_delay_ns) : 0;

	return oldest_ts <= safe_threshold;
}

} // namespace CommonLowLevelTracingKit::cmd::interface
