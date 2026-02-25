// Copyright (c) 2024, International Business Machines
// SPDX-License-Identifier: BSD-2-Clause-Patent

#ifndef CLLTK_SNAPSHOT_HEADER
#define CLLTK_SNAPSHOT_HEADER

#include <functional>
#include <optional>
#include <stddef.h>
#include <string>
#include <vector>

namespace CommonLowLevelTracingKit::snapshot
{

using write_function_t = std::function<std::optional<size_t>(const void *, size_t)>;
using verbose_function_t = std::function<void(const std::string &, const std::string &)>;

/**
 * Take a snapshot of all trace files from CLLTK_TRACING_PATH (or current directory).
 * @param func Write function to output the snapshot data
 * @param additional_tracepoints Additional trace messages to include (stored as JSON)
 * @param compress If true, compress the output with gzip
 * @param bucket_size Internal bucket size for tar packaging
 * @param verbose Optional verbose output function
 * @param recursive If true, recurse into subdirectories (default: true)
 */
std::optional<size_t> take_snapshot(write_function_t func,
									const std::vector<std::string> &additional_tracepoints = {},
									const bool compress = false, const size_t bucket_size = 4096,
									const verbose_function_t &verbose = {}, bool recursive = true);

/**
 * Take a snapshot of specific trace files.
 * @param func Write function to output the snapshot data
 * @param file_paths List of explicit file paths to include in the snapshot
 * @param additional_tracepoints Additional trace messages to include (stored as JSON)
 * @param compress If true, compress the output with gzip
 * @param bucket_size Internal bucket size for tar packaging
 * @param verbose Optional verbose output function
 */
std::optional<size_t>
take_snapshot_files(write_function_t func, const std::vector<std::string> &file_paths,
					const std::vector<std::string> &additional_tracepoints = {},
					const bool compress = false, const size_t bucket_size = 4096,
					const verbose_function_t &verbose = {});

} // namespace CommonLowLevelTracingKit::snapshot

#endif
