// Copyright (c) 2024, International Business Machines
// SPDX-License-Identifier: BSD-2-Clause-Patent

#include "CommonLowLevelTracingKit/interface.hpp"
#include "CommonLowLevelTracingKit/version.gen.h"
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

static void set_silent(void)
{
	static std::ostringstream deadEnd;
	std::cout.rdbuf(deadEnd.rdbuf());
	std::cerr.rdbuf(deadEnd.rdbuf());
}

static void set_path(const std::string_view &input)
{
	if (0 != setenv("CLLTK_TRACING_PATH", input.data(), 1))
		throw CLI::RuntimeError("could not set env variable CLLTK_TRACING_PATH", 1);
}

static void print_version(void)
{
	std::cout << "Common Low Level Tracing Kit " CLLTK_VERSION_STR << std::endl;
}

static std::unique_ptr<CLI::App> createMainApp(void)
{
	auto app = std::make_unique<CLI::App>();
	app->name("clltk");

	auto *const silent = app->add_flag_callback("--silent,-s", set_silent);

	app->add_option_function("--clltk_tracing_path,-C"s, std::function(set_path), "")
		->envname("CLLTK_TRACING_PATH")
		->check(CLI::ExistingPath);

	auto *const version = app->add_flag_callback("--version", print_version);

	version->excludes(silent);

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

int main(int argc, const char **argv)
{
	auto [app, lock] = CommonLowLevelTracingKit::cmd::interface::acquireMainApp();
	if (argc == 1) {
		std::cout << app.help() << std::endl;
		return 0;
	}
	CLI11_PARSE(app, argc, argv)
}