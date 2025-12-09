// Copyright (c) 2024, International Business Machines
// SPDX-License-Identifier: BSD-2-Clause-Patent

#ifndef _clltk_cmd_interface_HEADER__
#define _clltk_cmd_interface_HEADER__

#include "CLI/App.hpp"		  // IWYU pragma: export
#include "CLI/Config.hpp"	  // IWYU pragma: export
#include "CLI/Formatter.hpp"  // IWYU pragma: export
#include "CLI/Validators.hpp" // IWYU pragma: export
#if __has_include("CLI/ExtraValidators.hpp")
#include "CLI/ExtraValidators.hpp" // IWYU pragma: export
#endif
#include <CLI/Error.hpp>  // IWYU pragma: export
#include <CLI/Option.hpp> // IWYU pragma: export

#include <iostream>
#include <mutex>
#include <string>

typedef void (*init_fn)(void);
#define COMMAND_INIT(func) \
	__attribute__((retain, used, section("clltk_cmdinit"))) static init_fn _init_##func##_ptr = func

namespace CommonLowLevelTracingKit::cmd::interface
{
using MainAppHandle = std::pair<CLI::App &, std::unique_lock<std::mutex>>;
MainAppHandle acquireMainApp(void);

// ============================================================================
// Verbosity Control
// ============================================================================

enum class Verbosity { quiet, normal, verbose };

/// Get the current verbosity level
Verbosity get_verbosity(void);

/// Set the verbosity level (called by main.cpp during flag parsing)
void set_verbosity(Verbosity level);

/// Check if verbose mode is enabled
inline bool is_verbose(void)
{
	return get_verbosity() == Verbosity::verbose;
}

/// Check if quiet mode is enabled
inline bool is_quiet(void)
{
	return get_verbosity() == Verbosity::quiet;
}

/// Log an info message (shown in normal and verbose mode, hidden in quiet mode)
template <typename... Args>
void log_info(Args &&...args)
{
	if (!is_quiet()) {
		(std::cout << ... << std::forward<Args>(args)) << std::endl;
	}
}

/// Log a verbose message (only shown in verbose mode)
template <typename... Args>
void log_verbose(Args &&...args)
{
	if (is_verbose()) {
		(std::cout << ... << std::forward<Args>(args)) << std::endl;
	}
}

/// Log an error message (always shown, even in quiet mode)
template <typename... Args>
void log_error(Args &&...args)
{
	(std::cerr << ... << std::forward<Args>(args)) << std::endl;
}

// ============================================================================
// Signal Handling
// ============================================================================

/// Check if an interrupt signal (SIGINT/SIGTERM) has been received
bool is_interrupted(void);

/// Reset the interrupt flag (useful for cleanup before exit)
void reset_interrupt(void);

/// Install signal handlers for SIGINT and SIGTERM
void install_signal_handlers(void);

/// Get the path of the current output file being written (for cleanup on interrupt)
const std::string &get_current_output_file(void);

/// Set the current output file path (for cleanup on interrupt)
void set_current_output_file(const std::string &path);

/// Clear the current output file path
void clear_current_output_file(void);

/// RAII guard for managing output file cleanup on interrupt
class OutputFileGuard
{
  public:
	explicit OutputFileGuard(const std::string &path) { set_current_output_file(path); }
	~OutputFileGuard() { clear_current_output_file(); }

	OutputFileGuard(const OutputFileGuard &) = delete;
	OutputFileGuard &operator=(const OutputFileGuard &) = delete;
};

// ============================================================================
// Validators
// ============================================================================

namespace validator
{
struct TracebufferName : public CLI::Validator {
	TracebufferName(void);
};

} // namespace validator

} // namespace CommonLowLevelTracingKit::cmd::interface

#endif