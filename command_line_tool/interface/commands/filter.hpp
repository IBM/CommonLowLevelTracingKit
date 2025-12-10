// Copyright (c) 2024, International Business Machines
// SPDX-License-Identifier: BSD-2-Clause-Patent

#ifndef _clltk_cmd_filter_HEADER__
#define _clltk_cmd_filter_HEADER__

#include <CLI/App.hpp>
#include <boost/regex.hpp>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

#include "CommonLowLevelTracingKit/decoder/Tracepoint.hpp"

namespace CommonLowLevelTracingKit::cmd::interface
{

// Default regex pattern to match all tracebuffers
constexpr const char *default_filter_pattern = "^.*$";

/**
 * @brief Add tracebuffer filter option to a command
 *
 * Adds a standard -F/--filter option that uses boost::regex for filtering tracebuffer names.
 * This ensures consistent filtering behavior across all commands.
 *
 * @param command The CLI command to add the filter option to
 * @param filter_str Reference to a string that will store the filter pattern
 */
void add_filter_option(CLI::App *command, std::string &filter_str);

/**
 * @brief Check if a tracebuffer name matches the given filter regex
 *
 * @param name The tracebuffer name to check
 * @param filter_regex The compiled boost::regex to match against
 * @return true if the name matches the filter, false otherwise
 */
bool match_tracebuffer_filter(std::string_view name, const boost::regex &filter_regex);

using Tracepoint = CommonLowLevelTracingKit::decoder::Tracepoint;

/**
 * @brief Filter for individual tracepoints
 *
 * Provides filtering capabilities for tracepoints by:
 * - Time range (min/max timestamp)
 * - Process IDs (PIDs)
 * - Thread IDs (TIDs)
 * - Message content (substring or regex)
 * - Source file path (substring or regex)
 *
 * The filter is optimized for performance with:
 * - Pre-computed flags to skip inactive filters
 * - Ordered checks from cheapest to most expensive
 * - O(1) hash lookups for PID/TID matching
 */
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
	void configure() noexcept;

	// Set message filter (substring or regex)
	void set_msg_filter(const std::string &pattern, bool use_regex);

	// Set file filter (substring or regex)
	void set_file_filter(const std::string &pattern, bool use_regex);

	// Main filter function - optimized for hot path
	[[gnu::hot]] bool operator()(const Tracepoint &tp) const;
};

/**
 * @brief Add tracepoint filter options to a command
 *
 * Adds standard filter options for individual tracepoints:
 * - --pid: Filter by process ID(s)
 * - --tid: Filter by thread ID(s)
 * - --msg: Filter by message substring
 * - --msg-regex: Filter by message regex
 * - --file: Filter by file path substring
 * - --file-regex: Filter by file path regex
 *
 * @param command The CLI command to add the filter options to
 * @param filter_pids Reference to vector for storing PID filters
 * @param filter_tids Reference to vector for storing TID filters
 * @param filter_msg Reference to string for storing message substring filter
 * @param filter_msg_regex Reference to string for storing message regex filter
 * @param filter_file Reference to string for storing file substring filter
 * @param filter_file_regex Reference to string for storing file regex filter
 */
void add_tracepoint_filter_options(CLI::App *command, std::vector<uint32_t> &filter_pids,
								   std::vector<uint32_t> &filter_tids, std::string &filter_msg,
								   std::string &filter_msg_regex, std::string &filter_file,
								   std::string &filter_file_regex);

/**
 * @brief Configure a TracepointFilter from CLI option values
 *
 * @param filter The filter to configure
 * @param filter_pids Vector of PIDs to filter by
 * @param filter_tids Vector of TIDs to filter by
 * @param filter_msg Message substring filter
 * @param filter_msg_regex Message regex filter
 * @param filter_file File substring filter
 * @param filter_file_regex File regex filter
 */
void configure_tracepoint_filter(TracepointFilter &filter, const std::vector<uint32_t> &filter_pids,
								 const std::vector<uint32_t> &filter_tids,
								 const std::string &filter_msg, const std::string &filter_msg_regex,
								 const std::string &filter_file,
								 const std::string &filter_file_regex);

} // namespace CommonLowLevelTracingKit::cmd::interface

#endif // _clltk_cmd_filter_HEADER__