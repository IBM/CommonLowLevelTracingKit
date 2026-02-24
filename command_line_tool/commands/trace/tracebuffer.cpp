// Copyright (c) 2024, International Business Machines
// SPDX-License-Identifier: BSD-2-Clause-Patent

#include "CommonLowLevelTracingKit/tracing/tracing.h"
#include "commands/filter.hpp"
#include "commands/interface.hpp"
#include <boost/regex.hpp>
#include <filesystem>
#include <iostream>
#include <optional>
#include <stddef.h>
#include <string>
#include <vector>

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
		"Useful for resetting a tracebuffer to start fresh without recreating it.\n\n"
		"Options:\n"
		"  -b/--buffer  Clear a specific buffer by name\n"
		"  -F/--filter  Clear buffers matching a regex pattern\n"
		"  -a/--all     Clear all buffers (optionally filtered by -F)\n\n"
		"All operations prompt for confirmation unless -y/--yes is specified.");

	static std::string buffer_name{};
	static std::string filter_str =
		CommonLowLevelTracingKit::cmd::interface::default_filter_pattern;
	static bool all_flag = false;
	static bool yes_flag = false;

	auto buffer_opt = command
						  ->add_option("name,-b,--buffer", buffer_name,
									   "Name of the tracebuffer to clear.\n"
									   "Must match an existing tracebuffer at CLLTK_TRACING_PATH")
						  ->check(validator::TracebufferName{})
						  ->type_name("NAME");

	CommonLowLevelTracingKit::cmd::interface::add_filter_option(command, filter_str);

	auto all_opt = command->add_flag("-a,--all", all_flag,
									 "Clear all tracebuffers (optionally filtered by -F)");

	command->add_flag("-y,--yes", yes_flag, "Skip confirmation prompt");

	// Make --buffer mutually exclusive with --all
	buffer_opt->excludes(all_opt);
	all_opt->excludes(buffer_opt);

	command->callback([]() {
		CommonLowLevelTracingKit::cmd::interface::sync_path_to_library();
		const auto tracing_path = get_tracing_path();

		// Require at least one of --buffer, --all, or --filter
		if (buffer_name.empty() && !all_flag &&
			filter_str == CommonLowLevelTracingKit::cmd::interface::default_filter_pattern) {
			throw CLI::RuntimeError(
				"No buffer specified. Use --buffer <NAME>, --all, or --filter <PATTERN>.", 1);
		}

		// Helper to prompt for confirmation
		auto confirm = [](const std::string &prompt) -> bool {
			std::cout << prompt << " [y/N] ";
			std::cout.flush();
			std::string response;
			std::getline(std::cin, response);
			return !response.empty() && (response[0] == 'y' || response[0] == 'Y');
		};

		// Helper to find tracebuffer file path
		auto find_tracebuffer =
			[&tracing_path](const std::string &name) -> std::optional<std::filesystem::path> {
			for (const auto &ext : {".clltk_trace", ".clltk_ktrace"}) {
				auto path = tracing_path / (name + ext);
				if (std::filesystem::exists(path)) {
					return path;
				}
			}
			return std::nullopt;
		};

		// Helper to check file writability
		auto is_writable = [](const std::filesystem::path &path) -> bool {
			auto perms = std::filesystem::status(path).permissions();
			return (perms & std::filesystem::perms::owner_write) != std::filesystem::perms::none;
		};

		// Mode: clear by filter or all
		if (all_flag ||
			filter_str != CommonLowLevelTracingKit::cmd::interface::default_filter_pattern) {
			const boost::regex filter_regex{filter_str};

			// Collect all matching buffers
			std::vector<std::pair<std::string, std::filesystem::path>> to_clear;

			for (const auto &entry : std::filesystem::directory_iterator(tracing_path)) {
				if (!entry.is_regular_file())
					continue;

				const auto &path = entry.path();
				const auto ext = path.extension().string();

				if (ext == ".clltk_trace" || ext == ".clltk_ktrace") {
					const auto name = path.stem().string();

					if (CommonLowLevelTracingKit::cmd::interface::match_tracebuffer_filter(
							name, filter_regex)) {
						if (!is_writable(path)) {
							log_error("Skipping readonly tracebuffer: ", name);
							continue;
						}
						to_clear.emplace_back(name, path);
					}
				}
			}

			if (to_clear.empty()) {
				log_info("No tracebuffers found matching filter");
				return;
			}

			// Prompt for confirmation
			if (!yes_flag) {
				std::cout << "The following tracebuffer(s) will be cleared:\n";
				for (const auto &[name, path] : to_clear) {
					std::cout << "  - " << name << " (" << path.string() << ")\n";
				}
				std::cout << "\nClear " << to_clear.size() << " tracebuffer(s)?";
				if (!confirm("")) {
					log_info("Aborted");
					return;
				}
			}

			// Clear all matching buffers
			for (const auto &[name, path] : to_clear) {
				clltk_dynamic_tracebuffer_clear(name.c_str());
				log_verbose("Cleared tracebuffer '", name, "'");
			}
			log_info("Cleared ", to_clear.size(), " tracebuffer(s)");

		} else {
			// Mode: clear single buffer by name
			auto path_opt = find_tracebuffer(buffer_name);
			if (!path_opt) {
				throw CLI::RuntimeError("Tracebuffer not found: " + buffer_name, 1);
			}
			if (!is_writable(*path_opt)) {
				throw CLI::RuntimeError("Cannot clear readonly tracebuffer: " + buffer_name, 1);
			}

			// Prompt for confirmation
			if (!yes_flag) {
				std::cout << "Clear tracebuffer '" << buffer_name << "' (" << path_opt->string()
						  << ")?";
				if (!confirm("")) {
					log_info("Aborted");
					return;
				}
			}

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
