// Copyright (c) 2024, International Business Machines
// SPDX-License-Identifier: BSD-2-Clause-Patent

#include "ordered_buffer.hpp"

#include <algorithm>

namespace CommonLowLevelTracingKit::cmd::live
{

OrderedBuffer::OrderedBuffer(size_t max_size, uint64_t order_delay_ns)
	: m_max_size(max_size), m_order_delay_ns(order_delay_ns)
{
}

bool OrderedBuffer::push(TracepointPtr tp)
{
	if (!tp)
		return false;

	std::lock_guard lock(m_mutex);

	// Check if buffer is full
	if (m_max_size > 0 && m_heap.size() >= m_max_size) {
		// Drop oldest (which is at the front of min-heap)
		std::pop_heap(m_heap.begin(), m_heap.end(), TimestampCompare{});
		m_heap.pop_back();
		++m_stats.total_dropped;
	}

	m_heap.push_back(std::move(tp));
	std::push_heap(m_heap.begin(), m_heap.end(), TimestampCompare{});
	++m_stats.total_pushed;
	m_stats.current_size = m_heap.size();
	if (m_heap.size() > m_stats.high_water_mark) {
		m_stats.high_water_mark = m_heap.size();
	}

	// Notify waiting consumer
	m_cv.notify_one();

	return true;
}

void OrderedBuffer::update_watermark(uint64_t max_seen_ns)
{
	std::lock_guard lock(m_mutex);
	m_watermark_ns = max_seen_ns;
	m_cv.notify_one(); // Wake up consumer to check for ready tracepoints
}

void OrderedBuffer::finish()
{
	std::lock_guard lock(m_mutex);
	m_finished = true;
	m_cv.notify_all();
}

std::optional<TracepointPtr> OrderedBuffer::pop(std::chrono::milliseconds timeout)
{
	std::unique_lock lock(m_mutex);

	// Wait for a tracepoint to be ready or finish signal
	if (!m_cv.wait_for(lock, timeout, [this] { return has_ready_locked() || m_finished; })) {
		return std::nullopt; // Timeout
	}

	if (m_heap.empty()) {
		return std::nullopt;
	}

	if (m_finished) {
		// Flushing mode - output everything
		return pop_front_locked();
	}

	// Check watermark condition
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

	const uint64_t safe_threshold =
		(m_watermark_ns > m_order_delay_ns) ? (m_watermark_ns - m_order_delay_ns) : 0;

	while (!m_heap.empty()) {
		const uint64_t oldest_ts = m_heap.front()->timestamp_ns;

		if (m_finished || oldest_ts <= safe_threshold) {
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
	std::lock_guard lock(m_mutex);
	return m_finished && m_heap.empty();
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
	if (m_finished)
		return true;

	const uint64_t oldest_ts = m_heap.front()->timestamp_ns;
	const uint64_t safe_threshold =
		(m_watermark_ns > m_order_delay_ns) ? (m_watermark_ns - m_order_delay_ns) : 0;

	return oldest_ts <= safe_threshold;
}

} // namespace CommonLowLevelTracingKit::cmd::live
