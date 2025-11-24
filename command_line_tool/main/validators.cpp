// Copyright (c) 2024, International Business Machines
// SPDX-License-Identifier: BSD-2-Clause-Patent

#include "commands/interface.hpp"
#include <functional>
#include <regex>
#include <string>
#include <vector>

using namespace std::string_literals;

CommonLowLevelTracingKit::cmd::interface::validator::TracebufferName::TracebufferName(void)
	: CLI::Validator("BufferName")
{
	static const std::regex pattern{std::string{"^[a-zA-Z][a-zA-Z0-9_]{0,256}$"}};
	func_ = [](const std::string &filename) {
		if (std::regex_match(filename, pattern))
			return std::string(); // valid
		else
			return "invalid tracebuffer name"s;
	};
}
