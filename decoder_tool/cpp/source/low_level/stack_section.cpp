// Copyright (c) 2024, International Business Machines
// SPDX-License-Identifier: BSD-2-Clause-Patent

#include "stack_section.hpp"
#include "meta_parser.hpp"

#include <cstring>
#include <span>

namespace CommonLowLevelTracingKit::decoder::source {

	uint64_t StackSectionReader::getStackBodySize(const FilePart &file_part,
												  uint64_t stack_offset) {
		const uint64_t body_size_offset = stack_offset + stack_layout::HEADER_BODY_SIZE_OFFSET;
		return file_part.get<uint64_t>(body_size_offset);
	}

	StackEntryCollection StackSectionReader::read(const FilePart &file_part,
												  uint64_t stack_offset) {
		StackEntryCollection entries;

		const uint64_t body_size = getStackBodySize(file_part, stack_offset);
		if (body_size == 0) { return entries; }

		const uint64_t body_start = stack_offset + stack_layout::HEADER_SIZE;
		uint64_t offset = 0;

		while (offset < body_size) {
			const uint64_t entry_file_offset = body_start + offset;
			if (offset + stack_layout::ENTRY_HEADER_SIZE > body_size) { break; }

			StackEntry entry;
			entry.file_offset = entry_file_offset;

			const auto hash_lo =
				file_part.get<uint64_t>(entry_file_offset + stack_layout::ENTRY_MD5_OFFSET);
			const auto hash_hi =
				file_part.get<uint64_t>(entry_file_offset + stack_layout::ENTRY_MD5_OFFSET + 8);
			std::memcpy(entry.md5_hash.data(), &hash_lo, 8);
			std::memcpy(entry.md5_hash.data() + 8, &hash_hi, 8);

			entry.body_size =
				file_part.get<uint32_t>(entry_file_offset + stack_layout::ENTRY_BODY_SIZE_OFFSET);
			entry.crc = file_part.get<uint8_t>(entry_file_offset + stack_layout::ENTRY_CRC_OFFSET);

			const uint64_t total_entry_size = stack_layout::ENTRY_HEADER_SIZE + entry.body_size;
			if (offset + total_entry_size > body_size) { break; }

			if (entry.body_size > 0) {
				entry.body.resize(entry.body_size);
				const uint64_t body_offset = entry_file_offset + stack_layout::ENTRY_HEADER_SIZE;
				for (uint32_t i = 0; i < entry.body_size; ++i) {
					entry.body[i] = file_part.get<uint8_t>(body_offset + i);
				}
			}

			entries.push_back(std::move(entry));
			offset += total_entry_size;
		}

		return entries;
	}

	MetaEntryInfoCollection StackSectionReader::parseMetaEntries(const FilePart &file_part,
																 uint64_t stack_offset) {
		MetaEntryInfoCollection result;

		const auto stack_entries = read(file_part, stack_offset);

		for (const auto &stack_entry : stack_entries) {
			if (stack_entry.body.empty()) { continue; }

			const std::span<const uint8_t> body_span(stack_entry.body.data(),
													 stack_entry.body.size());
			const uint64_t body_file_offset =
				stack_entry.file_offset + stack_layout::ENTRY_HEADER_SIZE;

			auto parsed = MetaParser::parse(body_span, body_file_offset);
			for (auto &entry : parsed) { result.push_back(std::move(entry)); }
		}

		return result;
	}

} // namespace CommonLowLevelTracingKit::decoder::source
