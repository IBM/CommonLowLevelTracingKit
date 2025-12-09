// Copyright (c) 2024, International Business Machines
// SPDX-License-Identifier: BSD-2-Clause-Patent

#include "filter.hpp"
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
	// Use boost::regex_match to check if the name matches the filter
	return boost::regex_match(name.begin(), name.end(), filter_regex);
}

} // namespace CommonLowLevelTracingKit::cmd::interface