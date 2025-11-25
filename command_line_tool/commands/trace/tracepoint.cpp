// Copyright (c) 2024, International Business Machines
// SPDX-License-Identifier: BSD-2-Clause-Patent

#include "CommonLowLevelTracingKit/tracing/tracing.h"
#include "commands/interface.hpp"
#include <cstdint>
#include <stddef.h>
#include <string>

using namespace std::string_literals;

using namespace CommonLowLevelTracingKit::cmd::interface;

static void add_create_tracepoint_command(CLI::App &app)
{
	// trace command
	const std::string description =
		"Tracing a string to tracebuffer as a dynamic tracepoint.\nWith the possibility to add "
		"line number, file name, tid and pid.";

	CLI::App *const command = app.add_subcommand("tp", description);
	command->alias("tracepoint");

	static std::string tracebuffer{};
	command
		->add_option("tracebuffer,--tracebuffer,--tb", tracebuffer,
					 "The tracebuffer to which this tracepoint should be added. It does not yet "
					 "exist. it will be created with a ringbuffer size of 10kByte.")
		->required()
		->check(validator::TracebufferName{});

	static size_t size{512000};
	command
		->add_option("--tracebuffer-size", size,
					 "The size of the ringbuffer in this tracebuffer in bytes. (one tracepoint "
					 "entry, without any arguments will be ~ 32bytes)")
		->capture_default_str()
		->transform(CLI::AsSizeValue{false});

	static std::string message{};
	command
		->add_option("message,--message,--msg,-m", message,
					 "The ascii message string which should be traced")
		->required();

	static std::string filename{};
	command->add_option(
		"--file,-f", filename,
		"The source file from where this was traced. Otherwise it will be set to \"<unknown>\".");

	static size_t line{};
	command->add_option(
		"--line,-l", line,
		"The source file line number where this was traced. Otherwise it will be set to 0.");

	static uint32_t tid{};
	command->add_option("--tid,-t", tid,
						"The thread id where this was traced. Otherwise the command line tool "
						"thread id will be used");

	static uint32_t pid{};
	command->add_option("--pid,-p", pid,
						"The process id where this was traced. Otherwise the command line tool "
						"process id will be used");

	command->callback([]() {
		clltk_dynamic_tracepoint_execution(tracebuffer.c_str(), filename.c_str(), line, pid, tid,
										   "%s", message.c_str());
	});
}

static void init_function() noexcept
{
	auto [app, lock] = CommonLowLevelTracingKit::cmd::interface::acquireMainApp();
	add_create_tracepoint_command(app);
}
COMMAND_INIT(init_function);
