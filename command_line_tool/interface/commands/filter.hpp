// Copyright (c) 2024, International Business Machines
// SPDX-License-Identifier: BSD-2-Clause-Patent

#ifndef _clltk_cmd_filter_HEADER__
#define _clltk_cmd_filter_HEADER__

#include <CLI/App.hpp>
#include <boost/regex.hpp>
#include <string>
#include <string_view>

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

} // namespace CommonLowLevelTracingKit::cmd::interface

#endif // _clltk_cmd_filter_HEADER__