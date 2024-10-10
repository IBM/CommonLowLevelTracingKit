// Copyright (c) 2024, International Business Machines
// SPDX-License-Identifier: BSD-2-Clause-Patent

#include "CommonLowLevelTracingKit/interface.hpp"
#include <functional>
#include <regex>
#include <string>
#include <vector>

using namespace std::string_literals;

CommonLowLevelTracingKit::cmd::interface::validator::TracebufferName::TracebufferName(void)
	: CLI::Validator("BufferName")
{
	func_ = [](const std::string &filename) {
		std::regex pattern{"^[a-zA-Z][a-zA-Z0-9_]*$"};
		if (std::regex_match(filename, pattern))
			return std::string(); // valid
		else
			return "invalid tracebuffer name"s;
	};
}
