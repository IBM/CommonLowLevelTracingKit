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

	// create tracebuffer command
	const std::string description{
		"Creating a new tracebuffer, if not already exists.\nWith a given size. At "
		"CLLTK_TRACING_PATH.\nThis tool is only able to create user space tracebuffers."};

	CLI::App *const command = app.add_subcommand("tb", description);
	command->alias("tracebuffer");

	static std::string tracebuffer{};
	command
		->add_option(
			"name,--name,-n", tracebuffer,
			"Will be the name of this tracebuffer and should therefore be unique for this system.")
		->check(validator::TracebufferName{})
		->required();

	static size_t size{512000};
	command
		->add_option("size,--size,-s", size,
					 "The size of the ringbuffer in this tracebuffer in bytes. (one "
					 "tracepoint entry, without any arguments will be ~ 32bytes)")
		->capture_default_str()
		->transform(CLI::AsSizeValue{false})
		->required();

	command->callback([]() { clltk_dynamic_tracebuffer_creation(tracebuffer.c_str(), size); });
}

static int init_function() noexcept
{
	auto [app, lock] = CommonLowLevelTracingKit::cmd::interface::acquireMainApp();
	add_create_tracebuffer_command(app);
	return 0;
}

static const int dummy = init_function();
