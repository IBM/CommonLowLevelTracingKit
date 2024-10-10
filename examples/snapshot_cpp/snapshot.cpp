// Copyright (c) 2024, International Business Machines
// SPDX-License-Identifier: BSD-2-Clause-Patent

#include "CommonLowLevelTracingKit/snapshot.hpp"
#include <fstream> // IWYU pragma: keep
#include <iostream>
#include <stdlib.h>
#include <string>
#include <tuple>
#include <vector>

int main(int argc, const char **argv)
{
	std::ignore = argc;
	std::ignore = argv;

	std::vector<std::string> tracepoints = {};
	tracepoints.emplace_back("additional infos");

	std::ofstream tar("./trace.clltk_traces", std::ios::trunc);
	if (!tar.is_open()) {
		std::cerr << "could not open ./trace.clltk_traces file" << std::endl;
		exit(EXIT_FAILURE);
	}
	const auto tar_write = [&tar](const void *data, size_t size) -> std::optional<size_t> {
		tar.write(static_cast<const char *>(data), static_cast<std::streamsize>(size));
		return size;
	};

	std::ofstream comp("./trace_compressed.clltk_traces", std::ios::trunc);
	if (not comp.is_open()) {
		std::cerr << "could not open ./trace_compressed.clltk_traces file" << std::endl;
		exit(EXIT_FAILURE);
	}
	const auto comp_write = [&comp](const void *data, size_t size) -> std::optional<size_t> {
		comp.write(static_cast<const char *>(data), static_cast<std::streamsize>(size));
		return size;
	};

	if (not CommonLowLevelTracingKit::snapshot::take_snapshot(tar_write, tracepoints).has_value()) {
		std::cerr << "take_snapshot failed" << std::endl;
	}
	if (not CommonLowLevelTracingKit::snapshot::take_snapshot_compressed(comp_write, tracepoints)
				.has_value()) {
		std::cerr << "take_snapshot_compressed failed" << std::endl;
	}

	tar.close();
	comp.close();
	return 0;
}
