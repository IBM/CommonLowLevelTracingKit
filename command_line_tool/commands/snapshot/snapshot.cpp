// Copyright (c) 2024, International Business Machines
// SPDX-License-Identifier: BSD-2-Clause-Patent

#include "CommonLowLevelTracingKit/snapshot.hpp"
#include "CommonLowLevelTracingKit/interface.hpp"
#include <iostream>
#include <optional>
#include <stdexcept>
#include <stdint.h>
#include <stdio.h>
#include <string>
#include <vector>

using namespace std::string_literals;

using namespace CommonLowLevelTracingKit::cmd::interface;

static void take_snapshot(const std::string &filenamem, const std::vector<std::string> &tracepoints,
						  const bool compress, const size_t bucket_size,
						  const CommonLowLevelTracingKit::snapshot::verbose_function_t &verbose)
{
	std::ofstream output_file(filenamem, std::ios::out | std::ios::binary | std::ios::trunc);
	if (!output_file) {
		std::cout << "failed" << std::endl;
		CLI::RuntimeError(1);
	}
	const CommonLowLevelTracingKit::snapshot::write_function_t write_func =
		[&](const void *data, size_t size) -> std::optional<size_t> {
		output_file.write(static_cast<const char *>(data), size);
		return size;
	};
	CommonLowLevelTracingKit::snapshot::take_snapshot(write_func, tracepoints, compress,
													  bucket_size, verbose);
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
	CLI::App *const command = app.add_subcommand("sp", "Take a snapshot");
	command->alias("snapshot");

	static bool compress = false;
	command->add_flag("--compress,-z", compress, "compress snapshot");

	static CommonLowLevelTracingKit::snapshot::verbose_function_t verbose{};
	command->add_flag_callback("--verbose,-v", [&]() { verbose = verbose_func; });

	static std::string output_file_name{"traces.snapshot_library"};
	command->add_option("--output,-f", output_file_name, "")->capture_default_str();

	static std::vector<std::string> tracepoints;
	command->add_option("tracepoints", tracepoints, "additional tracepoints")->expected(0, -1);

	static uint64_t bucket_size{4096};
	command->add_option("--bucket_size", bucket_size, "")->capture_default_str();

	command->callback(
		[&]() { take_snapshot(output_file_name, tracepoints, compress, bucket_size, verbose); });
}

static int init_function() noexcept
{
	auto [app, lock] = CommonLowLevelTracingKit::cmd::interface::acquireMainApp();
	add_snapshot_command(app);
	return 0;
}

static const int dummy = init_function();
