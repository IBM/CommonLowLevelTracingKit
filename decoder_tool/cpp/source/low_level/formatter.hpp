#pragma once
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <ffi.h>
#include <span>
#include <string>
#include <string_view>

namespace CommonLowLevelTracingKit::decoder::source::formatter {

	std::string printf(const std::string_view format, const std::span<const char> &types,
					   const std::span<const uint8_t> &args_raw, bool foreign_endian = false);
	std::string dump(const std::string_view format, const std::span<const char> &types,
					 const std::span<const uint8_t> &args_raw, bool foreign_endian = false);

} // namespace CommonLowLevelTracingKit::decoder::source::formatter