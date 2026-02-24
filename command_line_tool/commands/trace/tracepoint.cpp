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
	CLI::App *const command =
		app.add_subcommand("trace", "Trace a single message to a tracebuffer");
	command->alias("tp");
	command->alias("tracepoint");
	command->description(
		"Write a single string message as a dynamic tracepoint to a tracebuffer.\n"
		"Useful for manual trace injection from scripts, shell commands, or external programs.\n"
		"Can include optional metadata: source file, line number, process ID, and thread ID.");

	static std::string buffer_name{};
	command
		->add_option("buffer,-b,--buffer,--tracebuffer,--tb", buffer_name,
					 "Target tracebuffer name.\n"
					 "If the tracebuffer does not exist, it will be created automatically")
		->required()
		->check(validator::TracebufferName{})
		->type_name("NAME");

	static size_t size{512000};
	command
		->add_option("-s,--size,--tracebuffer-size", size,
					 "Ring buffer size in bytes if creating a new tracebuffer.\n"
					 "One basic tracepoint is approximately 32 bytes.\n"
					 "Supports size suffixes: K, M, G (e.g., 512K, 1M)")
		->capture_default_str()
		->transform(CLI::AsSizeValue{false})
		->check([](const std::string &val) -> std::string {
			try {
				size_t parsed = std::stoull(val);
				if (parsed == 0) {
					return "size must be greater than zero";
				}
			} catch (...) {
				// Let CLI11 handle parsing errors
			}
			return "";
		})
		->type_name("SIZE");

	static std::string message{};
	command
		->add_option("message,-m,--message,--msg", message,
					 "ASCII message string to trace.\n"
					 "This is the main content of the tracepoint")
		->required()
		->type_name("TEXT");

	static std::string filename{};
	command
		->add_option("-f,--file", filename,
					 "Source file name to associate with this tracepoint.\n"
					 "Defaults to \"<unknown>\" if not specified")
		->type_name("FILE");

	static size_t line{};
	command
		->add_option("-l,--line", line,
					 "Source line number to associate with this tracepoint.\n"
					 "Defaults to 0 if not specified")
		->type_name("LINE");

	static uint32_t tid{};
	command
		->add_option("--tid,-t", tid,
					 "Thread ID to associate with this tracepoint.\n"
					 "Defaults to the clltk process's thread ID if not specified")
		->type_name("TID");

	static uint32_t pid{};
	command
		->add_option("--pid,-p", pid,
					 "Process ID to associate with this tracepoint.\n"
					 "Defaults to the clltk process's PID if not specified")
		->type_name("PID");

	command->callback([]() {
		CommonLowLevelTracingKit::cmd::interface::sync_path_to_library();
		clltk_dynamic_tracepoint_execution(buffer_name.c_str(), filename.c_str(), line, pid, tid,
										   "%s", message.c_str());
		log_verbose("Traced message to ", buffer_name);
	});
}

static void init_function() noexcept
{
	auto [app, lock] = CommonLowLevelTracingKit::cmd::interface::acquireMainApp();
	add_create_tracepoint_command(app);
}
COMMAND_INIT(init_function);
