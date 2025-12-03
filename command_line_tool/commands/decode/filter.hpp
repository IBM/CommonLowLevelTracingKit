// Copyright (c) 2024, International Business Machines
// SPDX-License-Identifier: BSD-2-Clause-Patent

#ifndef CLLTK_CMD_DECODE_FILTER_HPP
#define CLLTK_CMD_DECODE_FILTER_HPP

#include <boost/regex.hpp>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_set>

#include "CommonLowLevelTracingKit/decoder/Tracepoint.hpp"

namespace CommonLowLevelTracingKit::cmd::decode
{

using Tracepoint = CommonLowLevelTracingKit::decoder::Tracepoint;

struct TracepointFilter {
	// Pre-computed flags (set via configure())
	bool has_any_filter = false;
	bool has_time_filter = false;
	bool has_pid_filter = false;
	bool has_tid_filter = false;
	bool has_msg_filter = false;
	bool has_file_filter = false;

	// Time range
	uint64_t time_min = 0;
	uint64_t time_max = UINT64_MAX;

	// Process/Thread IDs - O(1) lookup
	std::unordered_set<uint32_t> pids;
	std::unordered_set<uint32_t> tids;

	// String filters
	std::string msg_substr;
	std::string file_substr;
	boost::regex msg_regex;
	boost::regex file_regex;
	bool msg_use_regex = false;
	bool file_use_regex = false;

	// Call after setting filter values to compute flags
	void configure() noexcept
	{
		has_time_filter = (time_min > 0 || time_max < UINT64_MAX);
		has_pid_filter = !pids.empty();
		has_tid_filter = !tids.empty();
		has_msg_filter = !msg_substr.empty() || msg_use_regex;
		has_file_filter = !file_substr.empty() || file_use_regex;
		has_any_filter = has_time_filter || has_pid_filter || has_tid_filter || has_msg_filter ||
						 has_file_filter;
	}

	// Set message filter (substring or regex)
	void set_msg_filter(const std::string &pattern, bool use_regex)
	{
		if (pattern.empty())
			return;
		msg_use_regex = use_regex;
		if (use_regex)
			msg_regex = boost::regex{pattern};
		else
			msg_substr = pattern;
	}

	// Set file filter (substring or regex)
	void set_file_filter(const std::string &pattern, bool use_regex)
	{
		if (pattern.empty())
			return;
		file_use_regex = use_regex;
		if (use_regex)
			file_regex = boost::regex{pattern};
		else
			file_substr = pattern;
	}

	// Main filter function - optimized for hot path
	[[gnu::hot]] inline bool operator()(const Tracepoint &tp) const noexcept
	{
		// Fast path: no filters active
		if (!has_any_filter) [[likely]]
			return true;

		// Cheapest checks first: integer comparisons
		if (has_time_filter) {
			if (tp.timestamp_ns < time_min || tp.timestamp_ns > time_max)
				return false;
		}

		// O(1) hash lookups
		if (has_pid_filter && !pids.contains(tp.pid()))
			return false;
		if (has_tid_filter && !tids.contains(tp.tid()))
			return false;

		// Expensive string checks last
		if (has_msg_filter) {
			const auto msg = tp.msg();
			if (msg_use_regex) {
				if (!boost::regex_search(msg.begin(), msg.end(), msg_regex))
					return false;
			} else {
				if (msg.find(msg_substr) == std::string_view::npos)
					return false;
			}
		}

		if (has_file_filter) {
			const auto file = tp.file();
			if (file_use_regex) {
				if (!boost::regex_search(file.begin(), file.end(), file_regex))
					return false;
			} else {
				if (file.find(file_substr) == std::string_view::npos)
					return false;
			}
		}

		return true;
	}
};

} // namespace CommonLowLevelTracingKit::cmd::decode

#endif // CLLTK_CMD_DECODE_FILTER_HPP
