// Copyright (c) 2024, International Business Machines
// SPDX-License-Identifier: BSD-2-Clause-Patent

#ifndef DECODER_TOOL_SOURCE_META_PARSER_HEADER
#define DECODER_TOOL_SOURCE_META_PARSER_HEADER

#include "CommonLowLevelTracingKit/decoder/Inline.hpp"
#include "CommonLowLevelTracingKit/decoder/Meta.hpp"

#include <cstdint>
#include <span>

namespace CommonLowLevelTracingKit::decoder::source {

	class MetaParser final {
	  public:
		static MetaEntryInfoCollection parse(std::span<const uint8_t> data,
											 uint64_t base_offset = 0);
		static size_t parseOne(std::span<const uint8_t> data, size_t offset, MetaEntryInfo &entry);
		CONST_INLINE static bool isValidMagic(uint8_t byte) { return byte == MAGIC_BYTE; }

		static constexpr uint8_t MAGIC_BYTE = '{';
		static constexpr size_t OFFSET_MAGIC = 0;
		static constexpr size_t OFFSET_SIZE = 1;
		static constexpr size_t OFFSET_TYPE = 5;
		static constexpr size_t OFFSET_LINE = 6;
		static constexpr size_t OFFSET_ARG_COUNT = 10;
		static constexpr size_t OFFSET_ARG_TYPES = 11;
		static constexpr size_t MIN_ENTRY_SIZE = 12;

	  private:
		MetaParser() = delete;
	};

} // namespace CommonLowLevelTracingKit::decoder::source

#endif
