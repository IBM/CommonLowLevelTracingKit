// Copyright (c) 2024, International Business Machines
// SPDX-License-Identifier: BSD-2-Clause-Patent

#ifndef DECODER_TOOL_SOURCE_STACK_SECTION_HEADER
#define DECODER_TOOL_SOURCE_STACK_SECTION_HEADER

#include "CommonLowLevelTracingKit/decoder/Inline.hpp"
#include "CommonLowLevelTracingKit/decoder/Meta.hpp"
#include "file.hpp"

#include <array>
#include <cstdint>
#include <vector>

namespace CommonLowLevelTracingKit::decoder::source {

	namespace stack_layout {
		constexpr size_t HEADER_SIZE = 120;
		constexpr size_t HEADER_VERSION_OFFSET = 0;
		constexpr size_t HEADER_BODY_SIZE_OFFSET = 112;

		constexpr size_t ENTRY_MD5_OFFSET = 0;
		constexpr size_t ENTRY_MD5_SIZE = 16;
		constexpr size_t ENTRY_RESERVED_OFFSET = 16;
		constexpr size_t ENTRY_RESERVED_SIZE = 8;
		constexpr size_t ENTRY_BODY_SIZE_OFFSET = 24;
		constexpr size_t ENTRY_CRC_OFFSET = 28;
		constexpr size_t ENTRY_HEADER_SIZE = 29;
	} // namespace stack_layout

	struct StackEntry {
		uint64_t file_offset;
		std::array<uint8_t, 16> md5_hash;
		uint32_t body_size;
		uint8_t crc;
		std::vector<uint8_t> body;
	};

	using StackEntryCollection = std::vector<StackEntry>;

	class StackSectionReader final {
	  public:
		static StackEntryCollection read(const FilePart &file_part, uint64_t stack_offset);
		static MetaEntryInfoCollection parseMetaEntries(const FilePart &file_part,
														uint64_t stack_offset);
		CONST_INLINE static uint64_t getStackOffset(const FilePart &file_part) {
			return file_part.get<uint64_t>(FILE_HEADER_STACK_OFFSET);
		}
		static uint64_t getStackBodySize(const FilePart &file_part, uint64_t stack_offset);

	  private:
		static constexpr size_t FILE_HEADER_STACK_OFFSET = 40;
		StackSectionReader() = delete;
	};

} // namespace CommonLowLevelTracingKit::decoder::source

#endif
