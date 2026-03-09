// Copyright (c) 2024, International Business Machines
// SPDX-License-Identifier: BSD-2-Clause-Patent

#include "CommonLowLevelTracingKit/build_info.gen.h"
#include "CommonLowLevelTracingKit/version.gen.h"
#include "commands/interface.hpp"
#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <string_view>

using namespace std::literals::string_literals;
using namespace std::literals::string_view_literals;
using namespace CommonLowLevelTracingKit::cmd::interface;

static void set_quiet(void)
{
	set_verbosity(Verbosity::quiet);
}

static void set_verbose(void)
{
	set_verbosity(Verbosity::verbose);
}

static void set_path(const std::string &input)
{
	set_path_option(input);
}

static void print_version(void)
{
	std::cout << CLLTK_BUILD_FULL_NAME << "\n";
	std::cout << "\n";
	std::cout << "Version:    " << CLLTK_BUILD_VERSION_STR;
#if CLLTK_BUILD_GIT_DIRTY
	std::cout << " (" << CLLTK_BUILD_GIT_HASH << "-dirty)";
#else
	std::cout << " (" << CLLTK_BUILD_GIT_HASH << ")";
#endif
	std::cout << "\n";
	std::cout << "License:    " << CLLTK_BUILD_LICENSE << "\n";
	std::cout << "Maintainer: " << CLLTK_BUILD_MAINTAINER << "\n";
	std::cout << "URL:        " << CLLTK_BUILD_URL << "\n";
	std::cout << "\n";
	std::cout << "Features:\n";
	std::cout << "  tracing:        " << (CLLTK_HAS_TRACING ? "enabled" : "disabled") << "\n";
	std::cout << "  snapshot:       " << (CLLTK_HAS_SNAPSHOT ? "enabled" : "disabled") << "\n";
	std::cout << "  cpp-decoder:    " << (CLLTK_HAS_CPP_DECODER ? "enabled" : "disabled") << "\n";
	std::cout << "  python-decoder: " << (CLLTK_HAS_PYTHON_DECODER ? "enabled" : "disabled")
			  << "\n";
	std::cout << "  kernel-tracing: " << (CLLTK_HAS_KERNEL_TRACING ? "enabled" : "disabled")
			  << "\n";
}

static std::unique_ptr<CLI::App> createMainApp(void)
{
	auto app = std::make_unique<CLI::App>();
	app->name("clltk");
	app->description("Common Low Level Tracing Kit - A high-performance tracing toolkit for "
					 "userspace and kernel tracing");

	auto *const quiet = app->add_flag_callback(
		"--quiet,-q", set_quiet, "Quiet mode: only show error messages, hide info and progress");

	auto *const verbose = app->add_flag_callback(
		"--verbose,-v", set_verbose, "Verbose mode: show detailed progress and info messages");

	quiet->excludes(verbose);

	app->add_option_function("-P,--path"s, std::function(set_path),
							 "Tracing path where tracebuffers are stored (default: .)")
		->envname("CLLTK_TRACING_PATH")
		->type_name("PATH");

	// Hidden alias for backwards compatibility with older versions that used -C
	app->add_option_function("-C,--clltk_tracing_path"s, std::function(set_path), "")->group("");

	auto *const version =
		app->add_flag_callback("-V,--version", print_version, "Print version information and exit");

	version->excludes(quiet);
	version->excludes(verbose);

	app->require_subcommand(0, 1);

	return app;
}

CommonLowLevelTracingKit::cmd::interface::MainAppHandle
CommonLowLevelTracingKit::cmd::interface::acquireMainApp(void)
{
	static auto mainApp = createMainApp();
	static std::mutex mainAppLock{};
	return {*mainApp, std::unique_lock{mainAppLock}};
}

void call_all_init_functions(void)
{
	extern init_fn __start_clltk_cmdinit;
	extern init_fn __stop_clltk_cmdinit;
	for (init_fn *p = &__start_clltk_cmdinit; p < &__stop_clltk_cmdinit; p++) {
		(*p)();
	}
}

int main(int argc, const char **argv)
{
	install_signal_handlers();
	call_all_init_functions();
	auto [app, lock] = acquireMainApp();
	if (argc == 1) {
		std::cout << app.help() << std::endl;
		return 0;
	}
	CLI11_PARSE(app, argc, argv)
}