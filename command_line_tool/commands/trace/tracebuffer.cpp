// Copyright (c) 2024, International Business Machines
// SPDX-License-Identifier: BSD-2-Clause-Patent

#include "CommonLowLevelTracingKit/tracing/tracing.h"
#include "commands/interface.hpp"
#include <stddef.h>
#include <string>

using namespace std::string_literals;

using namespace CommonLowLevelTracingKit::cmd::interface;

static void add_create_tracebuffer_command(CLI::App &app)
{
	CLI::App *const command = app.add_subcommand("tb", "Create a new tracebuffer");
	command->alias("tracebuffer");
	command->description(
		"Create a new userspace tracebuffer with a specified ring buffer size.\n"
		"The tracebuffer is created at CLLTK_TRACING_PATH.\n"
		"If the tracebuffer already exists, this command has no effect.\n"
		"Note: This tool only creates userspace tracebuffers, not kernel tracebuffers.");

	static std::string tracebuffer{};
	command
		->add_option("name,--name,-n", tracebuffer,
					 "Unique name for this tracebuffer.\n"
					 "Must start with a letter and contain only alphanumeric characters or "
					 "underscores.\n"
					 "Maximum length: 257 characters")
		->check(validator::TracebufferName{})
		->required()
		->type_name("NAME");

	static size_t size{512000};
	command
		->add_option("size,--size", size,
					 "Ring buffer size in bytes.\n"
					 "One basic tracepoint entry is approximately 32 bytes.\n"
					 "Supports size suffixes: K (kilobytes), M (megabytes), G (gigabytes).\n"
					 "Example: 512K, 1M, 2G")
		->capture_default_str()
		->transform(CLI::AsSizeValue{false})
		->required()
		->type_name("SIZE");

	command->callback([]() {
		clltk_dynamic_tracebuffer_creation(tracebuffer.c_str(), size);
		log_verbose("Created tracebuffer '", tracebuffer, "' with size ", size, " bytes");
	});
}

static void add_clear_tracebuffer_command(CLI::App &app)
{
	CLI::App *const command = app.add_subcommand("clear", "Clear all entries from a tracebuffer");
	command->description(
		"Clear all entries from an existing tracebuffer without deleting the file.\n"
		"The tracebuffer file is preserved; only the ring buffer content is discarded.\n"
		"Useful for resetting a tracebuffer to start fresh without recreating it.");

	static std::string tracebuffer{};
	command
		->add_option("name,--name,-n", tracebuffer,
					 "Name of the tracebuffer to clear.\n"
					 "Must match an existing tracebuffer at CLLTK_TRACING_PATH")
		->check(validator::TracebufferName{})
		->required()
		->type_name("NAME");

	command->callback([]() {
		clltk_dynamic_tracebuffer_clear(tracebuffer.c_str());
		log_verbose("Cleared tracebuffer '", tracebuffer, "'");
	});
}

static void init_function() noexcept
{
	auto [app, lock] = CommonLowLevelTracingKit::cmd::interface::acquireMainApp();
	add_create_tracebuffer_command(app);
	add_clear_tracebuffer_command(app);
}
COMMAND_INIT(init_function);
