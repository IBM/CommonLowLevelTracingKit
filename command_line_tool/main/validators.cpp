// Copyright (c) 2024, International Business Machines
// SPDX-License-Identifier: BSD-2-Clause-Patent

#include "commands/interface.hpp"
#include <atomic>
#include <csignal>
#include <filesystem>
#include <functional>
#include <regex>
#include <string>
#include <vector>

using namespace std::string_literals;

namespace CommonLowLevelTracingKit::cmd::interface
{

// ============================================================================
// Verbosity Control Implementation
// ============================================================================

static Verbosity g_verbosity = Verbosity::normal;

Verbosity get_verbosity(void)
{
	return g_verbosity;
}

void set_verbosity(Verbosity level)
{
	g_verbosity = level;
}

// ============================================================================
// Signal Handling Implementation
// ============================================================================

static std::atomic<bool> g_interrupted{false};
static std::string g_current_output_file{};

static void signal_handler(int /*signum*/)
{
	g_interrupted.store(true, std::memory_order_release);

	// Clean up partial output file if one is being written
	if (!g_current_output_file.empty()) {
		std::filesystem::remove(g_current_output_file);
	}
}

bool is_interrupted(void)
{
	return g_interrupted.load(std::memory_order_acquire);
}

void reset_interrupt(void)
{
	g_interrupted.store(false, std::memory_order_release);
}

void install_signal_handlers(void)
{
	std::signal(SIGINT, signal_handler);
	std::signal(SIGTERM, signal_handler);
}

const std::string &get_current_output_file(void)
{
	return g_current_output_file;
}

void set_current_output_file(const std::string &path)
{
	g_current_output_file = path;
}

void clear_current_output_file(void)
{
	g_current_output_file.clear();
}

// ============================================================================
// Validators Implementation
// ============================================================================

namespace validator
{

TracebufferName::TracebufferName(void) : CLI::Validator("BufferName")
{
	static const std::regex pattern{std::string{"^[a-zA-Z][a-zA-Z0-9_]{0,256}$"}};
	func_ = [](const std::string &filename) {
		if (std::regex_match(filename, pattern))
			return std::string(); // valid
		return "invalid tracebuffer name"s;
	};
}

} // namespace validator

} // namespace CommonLowLevelTracingKit::cmd::interface
