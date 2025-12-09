// Copyright (c) 2024, International Business Machines
// SPDX-License-Identifier: BSD-2-Clause-Patent

#include "CommonLowLevelTracingKit/snapshot/snapshot.hpp"
#include "commands/interface.hpp"
#include <iostream>
#include <optional>
#include <stdexcept>
#include <stdint.h>
#include <stdio.h>
#include <string>
#include <vector>

using namespace std::string_literals;

using namespace CommonLowLevelTracingKit::cmd::interface;

static void take_snapshot(const std::string &filename, const std::vector<std::string> &tracepoints,
						  const bool compress, const size_t bucket_size,
						  const CommonLowLevelTracingKit::snapshot::verbose_function_t &verbose)
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

	CommonLowLevelTracingKit::snapshot::take_snapshot(write_func, tracepoints, compress,
													  bucket_size, verbose);

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
	CLI::App *const command = app.add_subcommand("sp", "Take a snapshot of trace data");
	command->alias("snapshot");
	command->description(
		"Capture the current state of all tracebuffers and save to a portable archive file.\n"
		"The snapshot can be shared and analyzed later using the 'decode' command.\n"
		"By default, creates an uncompressed .clltk archive.");

	static bool compress = false;
	command->add_flag("--compress,-z", compress,
					  "Compress the snapshot using gzip (reduces file size)");

	// Note: Global --verbose/-v flag is handled by main.cpp
	// This local verbose is for detailed snapshot-specific output
	static CommonLowLevelTracingKit::snapshot::verbose_function_t verbose{};

	static std::string output_file_name{"snapshot.clltk"};
	command->add_option("--output,-o", output_file_name, "Output filename for the snapshot archive")
		->capture_default_str()
		->type_name("FILE");

	static std::vector<std::string> tracepoints;
	command
		->add_option("tracepoints", tracepoints,
					 "Additional tracepoint paths to include in the snapshot.\n"
					 "By default, includes all tracebuffers at CLLTK_TRACING_PATH")
		->expected(0, -1)
		->type_name("PATH...");

	static uint64_t bucket_size{4096};
	command
		->add_option("--bucket-size", bucket_size,
					 "Internal bucket size for snapshot packaging (bytes)")
		->capture_default_str()
		->type_name("SIZE");

	command->callback([&]() {
		// Use global verbose flag to enable detailed snapshot output
		auto verbose_fn = is_verbose() ? verbose_func : verbose;
		take_snapshot(output_file_name, tracepoints, compress, bucket_size, verbose_fn);
	});
}

static void init_function() noexcept
{
	auto [app, lock] = CommonLowLevelTracingKit::cmd::interface::acquireMainApp();
	add_snapshot_command(app);
}
COMMAND_INIT(init_function);
