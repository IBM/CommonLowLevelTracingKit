// Copyright (c) 2024, International Business Machines
// SPDX-License-Identifier: BSD-2-Clause-Patent

#include "CommonLowLevelTracingKit/tracing/tracing.h"
#include "commands/filter.hpp"
#include "commands/interface.hpp"
#include <boost/regex.hpp>
#include <filesystem>
#include <stddef.h>
#include <string>

using namespace std::string_literals;

using namespace CommonLowLevelTracingKit::cmd::interface;

static void add_create_tracebuffer_command(CLI::App &app)
{
	CLI::App *const command = app.add_subcommand("buffer", "Create a new tracebuffer");
	command->alias("tb");
	command->description(
		"Create a new userspace tracebuffer with a specified ring buffer size.\n"
		"The tracebuffer is created at CLLTK_TRACING_PATH (or -P path, or current directory).\n"
		"If the tracebuffer already exists, this command has no effect.\n"
		"Note: This tool only creates userspace tracebuffers, not kernel tracebuffers.");

	static std::string buffer_name{};
	command
		->add_option("buffer,-b,--buffer", buffer_name,
					 "Unique name for this tracebuffer.\n"
					 "Must start with a letter and contain only alphanumeric characters or "
					 "underscores.\n"
					 "Maximum length: 257 characters")
		->check(validator::TracebufferName{})
		->required()
		->type_name("NAME");

	static size_t size{512000};
	command
		->add_option("-s,--size", size,
					 "Ring buffer size in bytes.\n"
					 "One basic tracepoint entry is approximately 32 bytes.\n"
					 "Supports size suffixes: K (kilobytes), M (megabytes), G (gigabytes).\n"
					 "Example: 512K, 1M, 2G")
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

	command->callback([]() {
		CommonLowLevelTracingKit::cmd::interface::sync_path_to_library();
		clltk_dynamic_tracebuffer_creation(buffer_name.c_str(), size);
		log_verbose("Created tracebuffer '", buffer_name, "' with size ", size, " bytes");
	});
}

static void add_clear_tracebuffer_command(CLI::App &app)
{
	CLI::App *const command = app.add_subcommand("clear", "Clear all entries from a tracebuffer");
	command->alias("bx");
	command->description(
		"Clear all entries from an existing tracebuffer without deleting the file.\n"
		"The tracebuffer file is preserved; only the ring buffer content is discarded.\n"
		"Useful for resetting a tracebuffer to start fresh without recreating it.");

	static std::string buffer_name{};
	static bool all_buffers = false;
	static std::string filter_str =
		CommonLowLevelTracingKit::cmd::interface::default_filter_pattern;

	auto buffer_opt = command
						  ->add_option("name,-b,--buffer", buffer_name,
									   "Name of the tracebuffer to clear.\n"
									   "Must match an existing tracebuffer at CLLTK_TRACING_PATH")
						  ->check(validator::TracebufferName{})
						  ->type_name("NAME");

	auto all_opt =
		command->add_flag("-a,--all", all_buffers, "Clear all tracebuffers matching the filter");

	CommonLowLevelTracingKit::cmd::interface::add_filter_option(command, filter_str);

	// Make --buffer and --all mutually exclusive
	buffer_opt->excludes(all_opt);
	all_opt->excludes(buffer_opt);

	// Require one of them
	command->require_option(1, 2);

	command->callback([]() {
		CommonLowLevelTracingKit::cmd::interface::sync_path_to_library();
		const auto tracing_path = get_tracing_path();

		// Helper to check file writability
		auto check_writable = [&tracing_path](const std::string &name) -> std::filesystem::path {
			// Try both userspace and kernel trace file extensions
			for (const auto &ext : {".clltk_trace", ".clltk_ktrace"}) {
				auto path = tracing_path / (name + ext);
				if (std::filesystem::exists(path)) {
					// Check if file is writable
					auto perms = std::filesystem::status(path).permissions();
					if ((perms & std::filesystem::perms::owner_write) ==
						std::filesystem::perms::none) {
						throw CLI::RuntimeError("Cannot clear readonly tracebuffer: " + name, 1);
					}
					return path;
				}
			}
			throw CLI::RuntimeError("Tracebuffer not found: " + name, 1);
		};

		if (all_buffers) {
			// Clear all tracebuffers matching the filter
			const boost::regex filter_regex{filter_str};

			size_t cleared_count = 0;

			// Scan directory for tracebuffers
			for (const auto &entry : std::filesystem::directory_iterator(tracing_path)) {
				if (!entry.is_regular_file())
					continue;

				const auto &path = entry.path();
				const auto ext = path.extension().string();

				if (ext == ".clltk_trace" || ext == ".clltk_ktrace") {
					const auto name = path.stem().string();

					if (CommonLowLevelTracingKit::cmd::interface::match_tracebuffer_filter(
							name, filter_regex)) {
						// Check if file is writable
						auto perms = std::filesystem::status(path).permissions();
						if ((perms & std::filesystem::perms::owner_write) ==
							std::filesystem::perms::none) {
							log_error("Skipping readonly tracebuffer: ", name);
							continue;
						}
						clltk_dynamic_tracebuffer_clear(name.c_str());
						log_verbose("Cleared tracebuffer '", name, "'");
						cleared_count++;
					}
				}
			}

			if (cleared_count > 0) {
				log_info("Cleared ", cleared_count, " tracebuffer(s)");
			} else {
				log_info("No tracebuffers found matching filter");
			}
		} else {
			// Clear a single tracebuffer - check it exists and is writable
			check_writable(buffer_name);
			clltk_dynamic_tracebuffer_clear(buffer_name.c_str());
			log_verbose("Cleared tracebuffer '", buffer_name, "'");
		}
	});
}

static void init_function() noexcept
{
	auto [app, lock] = CommonLowLevelTracingKit::cmd::interface::acquireMainApp();
	add_create_tracebuffer_command(app);
	add_clear_tracebuffer_command(app);
}
COMMAND_INIT(init_function);
