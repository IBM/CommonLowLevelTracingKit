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

std::optional<size_t> take_snapshot(write_function_t,
									const std::vector<std::string> &additional_tracepoints = {},
									const bool compress = false, const size_t bucket_size = 4096,
									const verbose_function_t &verbose = {});

std::optional<size_t>
take_snapshot_compressed(write_function_t,
						 const std::vector<std::string> &additional_tracepoints = {},
						 const size_t bucket_size = 4096, const verbose_function_t &verbose = {});

} // namespace CommonLowLevelTracingKit::snapshot

#endif
