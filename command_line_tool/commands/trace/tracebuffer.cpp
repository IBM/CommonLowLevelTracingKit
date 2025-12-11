// Copyright (c) 2024, International Business Machines
// SPDX-License-Identifier: BSD-2-Clause-Patent

#include "CommonLowLevelTracingKit/tracing/tracing.h"
#include "commands/filter.hpp"
#include "commands/interface.hpp"
#include <boost/regex.hpp>
#include <filesystem>
#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>
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
		->type_name("SIZE");

	command->callback([]() {
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
		if (all_buffers) {
			// Clear all tracebuffers matching the filter
			const auto tracing_path = get_tracing_path();
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
			// Clear a single tracebuffer
			clltk_dynamic_tracebuffer_clear(buffer_name.c_str());
			log_verbose("Cleared tracebuffer '", buffer_name, "'");
		}
	});
}

static void add_list_tracebuffer_command(CLI::App &app)
{
	CLI::App *const command = app.add_subcommand("list", "List tracebuffers in the tracing path");
	command->alias("bl");
	command->description(
		"List all tracebuffers found at CLLTK_TRACING_PATH (or -P path, or current directory).\n"
		"Shows tracebuffer names and file paths.");

	static bool recursive = false;
	command->add_flag("-r,--recursive", recursive, "Recurse into subdirectories");

	static std::string filter_str =
		CommonLowLevelTracingKit::cmd::interface::default_filter_pattern;
	CommonLowLevelTracingKit::cmd::interface::add_filter_option(command, filter_str);

	static bool json_output = false;
	command->add_flag("-j,--json", json_output, "Output as JSON");

	command->callback([]() {
		const auto tracing_path = get_tracing_path();
		const boost::regex filter_regex{filter_str};

		std::vector<std::filesystem::path> found_buffers;

		auto scan_directory = [&](const std::filesystem::path &dir, bool recurse) {
			if (!std::filesystem::exists(dir) || !std::filesystem::is_directory(dir)) {
				log_error("Path does not exist or is not a directory: ", dir.string());
				return;
			}

			auto process_entry = [&](const std::filesystem::directory_entry &entry) {
				if (!entry.is_regular_file())
					return;
				const auto &path = entry.path();
				const auto ext = path.extension().string();
				if (ext == ".clltk_trace" || ext == ".clltk_ktrace") {
					const auto name = path.stem().string();
					if (CommonLowLevelTracingKit::cmd::interface::match_tracebuffer_filter(
							name, filter_regex)) {
						found_buffers.push_back(path);
					}
				}
			};

			if (recurse) {
				for (const auto &entry : std::filesystem::recursive_directory_iterator(dir)) {
					process_entry(entry);
				}
			} else {
				for (const auto &entry : std::filesystem::directory_iterator(dir)) {
					process_entry(entry);
				}
			}
		};

		scan_directory(tracing_path, recursive);

		if (json_output) {
			rapidjson::Document doc;
			doc.SetArray();
			auto &allocator = doc.GetAllocator();

			for (const auto &path : found_buffers) {
				rapidjson::Value obj;
				obj.SetObject();

				rapidjson::Value name;
				name.SetString(path.stem().string().c_str(), allocator);
				obj.AddMember("name", name, allocator);

				rapidjson::Value pathStr;
				pathStr.SetString(path.string().c_str(), allocator);
				obj.AddMember("path", pathStr, allocator);

				doc.PushBack(obj, allocator);
			}

			rapidjson::StringBuffer buffer;
			rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
			doc.Accept(writer);
			std::cout << buffer.GetString() << std::endl;
		} else {
			if (found_buffers.empty()) {
				log_info("No tracebuffers found in ", tracing_path.string());
			} else {
				for (const auto &path : found_buffers) {
					std::cout << path.stem().string() << "\t" << path.string() << std::endl;
				}
			}
		}
	});
}

static void init_function() noexcept
{
	auto [app, lock] = CommonLowLevelTracingKit::cmd::interface::acquireMainApp();
	add_create_tracebuffer_command(app);
	add_clear_tracebuffer_command(app);
	// Note: list command moved to decoder module (list.cpp) with enhanced statistics
}
COMMAND_INIT(init_function);
