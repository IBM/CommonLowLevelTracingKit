// Copyright (c) 2024, International Business Machines
// SPDX-License-Identifier: BSD-2-Clause-Patent

#ifndef CLLTK_CMD_DECODE_FILTER_HPP
#define CLLTK_CMD_DECODE_FILTER_HPP

#include <boost/regex.hpp>
#include <cstdint>
#include <string>
#include <unordered_set>

#include "CommonLowLevelTracingKit/decoder/Tracepoint.hpp"

namespace CommonLowLevelTracingKit::cmd::decode
{

using Tracepoint = CommonLowLevelTracingKit::decoder::Tracepoint;

struct TracepointFilter {
	// flags set by configure()
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

	void configure() noexcept;
	void set_msg_filter(const std::string &pattern, bool use_regex);
	void set_file_filter(const std::string &pattern, bool use_regex);

	[[gnu::hot]] bool operator()(const Tracepoint &tp) const;
};

} // namespace CommonLowLevelTracingKit::cmd::decode

#endif // CLLTK_CMD_DECODE_FILTER_HPP
