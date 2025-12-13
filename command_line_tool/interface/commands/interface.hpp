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

#include <cstdlib>
#include <filesystem>
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

const std::string &get_path_option(void);
void set_path_option(const std::string &path);

// Priority: -P option > CLLTK_TRACING_PATH env > "."
inline std::filesystem::path get_tracing_path(void)
{
	// 1. Check -P/--path option
	const auto &path_opt = get_path_option();
	if (!path_opt.empty()) {
		return path_opt;
	}

	// 2. Check CLLTK_TRACING_PATH environment variable
	const char *env_path = std::getenv("CLLTK_TRACING_PATH");
	if (env_path != nullptr && env_path[0] != '\0') {
		return env_path;
	}

	// 3. Default to current directory
	return ".";
}

// Forward declaration of C library function
extern "C" void clltk_set_tracing_path(const char *path);

// Sync the -P/--path option to the C tracing library.
// Call this before invoking C library functions that use the tracing path.
inline void sync_path_to_library(void)
{
	const auto &path_opt = get_path_option();
	if (!path_opt.empty()) {
		clltk_set_tracing_path(path_opt.c_str());
	}
}

enum class Verbosity { quiet, normal, verbose };

Verbosity get_verbosity(void);
void set_verbosity(Verbosity level);

inline bool is_verbose(void)
{
	return get_verbosity() == Verbosity::verbose;
}

inline bool is_quiet(void)
{
	return get_verbosity() == Verbosity::quiet;
}

template <typename... Args> void log_info(Args &&...args) // hidden in quiet mode
{
	if (!is_quiet()) {
		(std::cout << ... << std::forward<Args>(args)) << std::endl;
	}
}

template <typename... Args> void log_verbose(Args &&...args) // only in verbose mode
{
	if (is_verbose()) {
		(std::cout << ... << std::forward<Args>(args)) << std::endl;
	}
}

template <typename... Args> void log_error(Args &&...args) // always shown
{
	(std::cerr << ... << std::forward<Args>(args)) << std::endl;
}

bool is_interrupted(void);
void reset_interrupt(void);
void install_signal_handlers(void);

const std::string &get_current_output_file(void);
void set_current_output_file(const std::string &path);
void clear_current_output_file(void);

// RAII guard for output file cleanup on interrupt
class OutputFileGuard
{
  public:
	explicit OutputFileGuard(const std::string &path) { set_current_output_file(path); }
	~OutputFileGuard() { clear_current_output_file(); }

	OutputFileGuard(const OutputFileGuard &) = delete;
	OutputFileGuard &operator=(const OutputFileGuard &) = delete;
};

namespace validator
{
struct TracebufferName : public CLI::Validator {
	TracebufferName(void);
};

struct ExistingTracePath : public CLI::Validator {
	ExistingTracePath(void);
};

} // namespace validator

} // namespace CommonLowLevelTracingKit::cmd::interface

#endif