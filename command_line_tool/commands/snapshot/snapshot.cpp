// Copyright (c) 2024, International Business Machines
// SPDX-License-Identifier: BSD-2-Clause-Patent

#include "CommonLowLevelTracingKit/snapshot/snapshot.hpp"
#include "commands/filter.hpp"
#include "commands/interface.hpp"
#include <boost/regex.hpp>
#include <filesystem>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <stdint.h>
#include <stdio.h>
#include <string>
#include <vector>

using namespace std::string_literals;

using namespace CommonLowLevelTracingKit::cmd::interface;
namespace fs = std::filesystem;

static void take_snapshot(const std::string &filename, const std::vector<std::string> &tracepoints,
						  const bool compress, const size_t bucket_size,
						  const CommonLowLevelTracingKit::snapshot::verbose_function_t &verbose,
						  const boost::regex *filter_regex = nullptr, bool recursive = false)
{
	// Register output file for cleanup on interrupt
	OutputFileGuard output_guard(filename);

	std::ofstream output_file(filename, std::ios::out | std::ios::binary | std::ios::trunc);
	if (!output_file) {
		log_error("Failed to open output file: ", filename);
		throw CLI::RuntimeError(1);
	}

	const CommonLowLevelTracingKit::snapshot::write_function_t write_func =
		[&](const void *data, size_t size) -> std::optional<size_t> {
		if (is_interrupted()) {
			return std::nullopt; // Signal interruption to snapshot library
		}
		output_file.write(static_cast<const char *>(data), size);
		return size;
	};

	if (filter_regex) {
		// Use the filter to only include matching tracebuffers
		std::vector<std::string> filtered_tracebuffers;

		// Get the tracing path
		const auto tracing_path = get_tracing_path();

		// Helper to process a directory entry
		auto process_entry = [&](const fs::directory_entry &entry) {
			if (!entry.is_regular_file())
				return;

			const auto &path = entry.path();
			const auto ext = path.extension().string();

			if (ext == ".clltk_trace" || ext == ".clltk_ktrace") {
				const auto name = path.stem().string();

				if (CommonLowLevelTracingKit::cmd::interface::match_tracebuffer_filter(
						name, *filter_regex)) {
					filtered_tracebuffers.push_back(path.string());
				}
			}
		};

		// Scan directory for tracebuffers (with optional recursion)
		if (recursive) {
			for (const auto &entry : fs::recursive_directory_iterator(tracing_path)) {
				process_entry(entry);
			}
		} else {
			for (const auto &entry : fs::directory_iterator(tracing_path)) {
				process_entry(entry);
			}
		}

		// Use take_snapshot_files to only include the filtered files (not all from
		// CLLTK_TRACING_PATH)
		CommonLowLevelTracingKit::snapshot::take_snapshot_files(
			write_func, filtered_tracebuffers, tracepoints, compress, bucket_size, verbose);
	} else {
		// No filter, use all tracebuffers from CLLTK_TRACING_PATH
		CommonLowLevelTracingKit::snapshot::take_snapshot(write_func, tracepoints, compress,
														  bucket_size, verbose, recursive);
	}

	if (is_interrupted()) {
		log_info("Snapshot interrupted, partial file deleted");
	} else {
		log_verbose("Snapshot saved to ", filename);
	}
}

static void verbose_func(const std::string &stdout, const std::string &stderr)
{
	if (!stdout.empty())
		std::cout << stdout << std::endl;
	if (!stderr.empty())
		std::cerr << stderr << std::endl;
}

static void add_snapshot_command(CLI::App &app)
{
	CLI::App *const command = app.add_subcommand("snapshot", "Take a snapshot of trace data");
	command->alias("sp");
	command->description(
		"Capture the current state of all tracebuffers and save to a portable archive file.\n"
		"The snapshot can be shared and analyzed later using the 'decode' command.\n"
		"Captures from CLLTK_TRACING_PATH (or -P path, or current directory).\n"
		"By default, creates an uncompressed .clltk archive.");

	static bool compress = false;
	command->add_flag("-z,--compress", compress,
					  "Compress the snapshot using gzip (reduces file size)");

	// Note: Global --verbose/-v flag is handled by main.cpp
	// This local verbose is for detailed snapshot-specific output
	static CommonLowLevelTracingKit::snapshot::verbose_function_t verbose{};

	static std::string output_file_name{"snapshot.clltk"};
	command->add_option("-o,--output", output_file_name, "Output filename for the snapshot archive")
		->capture_default_str()
		->type_name("FILE");

	static std::vector<std::string> include_paths;
	command
		->add_option("--include", include_paths,
					 "Additional paths to include in the snapshot.\n"
					 "Can be specified multiple times")
		->type_name("PATH");

	static std::string filter_str =
		CommonLowLevelTracingKit::cmd::interface::default_filter_pattern;
	CommonLowLevelTracingKit::cmd::interface::add_filter_option(command, filter_str);

	static bool recursive = true;
	command->add_flag("-r,--recursive,!--no-recursive", recursive,
					  "Recurse into subdirectories (default: yes)");

	static uint64_t bucket_size{4096};
	command
		->add_option("--bucket-size", bucket_size,
					 "Internal bucket size for snapshot packaging (bytes)")
		->capture_default_str()
		->type_name("SIZE");

	command->callback([&]() {
		// Use global verbose flag to enable detailed snapshot output
		auto verbose_fn = is_verbose() ? verbose_func : verbose;

		// Check if a filter was specified
		if (filter_str != CommonLowLevelTracingKit::cmd::interface::default_filter_pattern) {
			const boost::regex filter_regex{filter_str};
			take_snapshot(output_file_name, include_paths, compress, bucket_size, verbose_fn,
						  &filter_regex, recursive);
		} else {
			take_snapshot(output_file_name, include_paths, compress, bucket_size, verbose_fn,
						  nullptr, recursive);
		}
	});
}

static void init_function() noexcept
{
	auto [app, lock] = CommonLowLevelTracingKit::cmd::interface::acquireMainApp();
	add_snapshot_command(app);
}
COMMAND_INIT(init_function);
