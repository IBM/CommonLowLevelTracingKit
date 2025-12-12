// Copyright (c) 2024, International Business Machines
// SPDX-License-Identifier: BSD-2-Clause-Patent

#include "commands/filter.hpp"
#include <CLI/App.hpp>

namespace CommonLowLevelTracingKit::cmd::interface
{

void add_filter_option(CLI::App *command, std::string &filter_str)
{
	// Initialize with the default pattern if empty
	if (filter_str.empty()) {
		filter_str = default_filter_pattern;
	}

	command->add_option("-F,--filter", filter_str, "Filter tracebuffers by name using regex")
		->capture_default_str()
		->type_name("REGEX");
}

bool match_tracebuffer_filter(std::string_view name, const boost::regex &filter_regex)
{
	// Use boost::regex_search to allow partial matches (more intuitive for users)
	// This enables patterns like "^Test" to match "TestAlpha", "TestBeta", etc.
	return boost::regex_search(name.begin(), name.end(), filter_regex);
}

void TracepointFilter::configure() noexcept
{
	has_time_filter = (time_min > 0 || time_max < UINT64_MAX);
	has_pid_filter = !pids.empty();
	has_tid_filter = !tids.empty();
	has_msg_filter = !msg_substr.empty() || msg_use_regex;
	has_file_filter = !file_substr.empty() || file_use_regex;
	has_any_filter =
		has_time_filter || has_pid_filter || has_tid_filter || has_msg_filter || has_file_filter;
}

void TracepointFilter::set_msg_filter(const std::string &pattern, bool use_regex)
{
	if (pattern.empty())
		return;
	msg_use_regex = use_regex;
	if (use_regex)
		msg_regex = boost::regex{pattern};
	else
		msg_substr = pattern;
}

void TracepointFilter::set_file_filter(const std::string &pattern, bool use_regex)
{
	if (pattern.empty())
		return;
	file_use_regex = use_regex;
	if (use_regex)
		file_regex = boost::regex{pattern};
	else
		file_substr = pattern;
}

bool TracepointFilter::operator()(const Tracepoint &tp) const
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

void add_tracepoint_filter_options(CLI::App *command, std::vector<uint32_t> &filter_pids,
								   std::vector<uint32_t> &filter_tids, std::string &filter_msg,
								   std::string &filter_msg_regex, std::string &filter_file,
								   std::string &filter_file_regex)
{
	command
		->add_option("--pid", filter_pids,
					 "Filter by process ID(s). Can be specified multiple times")
		->type_name("PID");
	command
		->add_option("--tid", filter_tids,
					 "Filter by thread ID(s). Can be specified multiple times")
		->type_name("TID");
	command->add_option("--msg", filter_msg, "Filter tracepoints containing this message substring")
		->type_name("TEXT");
	command
		->add_option("--msg-regex", filter_msg_regex,
					 "Filter tracepoints by message using ECMAScript regex")
		->type_name("REGEX");
	command
		->add_option("--file", filter_file,
					 "Filter tracepoints from files containing this path substring")
		->type_name("TEXT");
	command
		->add_option("--file-regex", filter_file_regex,
					 "Filter tracepoints by source file path using ECMAScript regex")
		->type_name("REGEX");
}

void configure_tracepoint_filter(TracepointFilter &filter, const std::vector<uint32_t> &filter_pids,
								 const std::vector<uint32_t> &filter_tids,
								 const std::string &filter_msg, const std::string &filter_msg_regex,
								 const std::string &filter_file,
								 const std::string &filter_file_regex)
{
	filter.pids.insert(filter_pids.begin(), filter_pids.end());
	filter.tids.insert(filter_tids.begin(), filter_tids.end());
	if (!filter_msg_regex.empty())
		filter.set_msg_filter(filter_msg_regex, true);
	else if (!filter_msg.empty())
		filter.set_msg_filter(filter_msg, false);
	if (!filter_file_regex.empty())
		filter.set_file_filter(filter_file_regex, true);
	else if (!filter_file.empty())
		filter.set_file_filter(filter_file, false);
	filter.configure();
}

void add_time_range_options(CLI::App *command, std::string &since_str, std::string &until_str)
{
	command
		->add_option("--since", since_str,
					 "Show tracepoints from this time onwards.\n"
					 "Formats:\n"
					 "  -5m, +30s       - relative to now\n"
					 "  now, now-1m     - explicit now anchor\n"
					 "  min, min+1h     - relative to trace start\n"
					 "  max, max-5m     - relative to trace end\n"
					 "  1234567890.5    - Unix timestamp\n"
					 "  2025-12-11T10:00:00 - ISO datetime")
		->type_name("TIME");

	command
		->add_option("--until", until_str,
					 "Show tracepoints up to this time.\n"
					 "(same formats as --since)")
		->type_name("TIME");
}

} // namespace CommonLowLevelTracingKit::cmd::interface
