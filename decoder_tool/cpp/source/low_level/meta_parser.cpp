// Copyright (c) 2024, International Business Machines
// SPDX-License-Identifier: BSD-2-Clause-Patent

#include "meta_parser.hpp"

#include <cstring>

namespace CommonLowLevelTracingKit::decoder::source {

	MetaEntryInfoCollection MetaParser::parse(std::span<const uint8_t> data, uint64_t base_offset) {
		MetaEntryInfoCollection result;

		size_t offset = 0;
		while (offset < data.size()) {
			if (!isValidMagic(data[offset])) {
				++offset;
				continue;
			}

			MetaEntryInfo entry;
			entry.offset = base_offset + offset;

			const size_t consumed = parseOne(data, offset, entry);
			if (consumed == 0) {
				++offset;
				continue;
			}

			result.push_back(std::move(entry));
			offset += consumed;
		}

		return result;
	}

	size_t MetaParser::parseOne(std::span<const uint8_t> data, size_t offset,
								MetaEntryInfo &entry) {
		const size_t remaining = data.size() - offset;
		if (remaining < MIN_ENTRY_SIZE) { return 0; }

		const uint8_t *const base = data.data() + offset;
		if (base[OFFSET_MAGIC] != MAGIC_BYTE) { return 0; }

		uint32_t entry_size = 0;
		std::memcpy(&entry_size, base + OFFSET_SIZE, sizeof(entry_size));
		if (entry_size < MIN_ENTRY_SIZE || entry_size > remaining) { return 0; }

		entry.size = entry_size;

		const uint8_t type_byte = base[OFFSET_TYPE];
		if (type_byte == static_cast<uint8_t>(MetaEntryType::Printf)) {
			entry.type = MetaEntryType::Printf;
		} else if (type_byte == static_cast<uint8_t>(MetaEntryType::Dump)) {
			entry.type = MetaEntryType::Dump;
		} else {
			entry.type = MetaEntryType::Unknown;
		}

		std::memcpy(&entry.line, base + OFFSET_LINE, sizeof(entry.line));
		entry.arg_count = base[OFFSET_ARG_COUNT];

		const size_t arg_types_array_size = entry.arg_count + 1;
		const size_t arg_types_end = OFFSET_ARG_TYPES + arg_types_array_size;
		if (arg_types_end >= entry_size) { return 0; }

		entry.arg_types =
			std::string(reinterpret_cast<const char *>(base + OFFSET_ARG_TYPES), entry.arg_count);

		const size_t file_offset = arg_types_end;
		const char *file_start = reinterpret_cast<const char *>(base + file_offset);
		const size_t max_file_len = entry_size - file_offset;
		const size_t file_len = strnlen(file_start, max_file_len);

		if (file_len >= max_file_len) { return 0; }
		entry.file = std::string(file_start, file_len);

		const size_t format_offset = file_offset + file_len + 1;
		if (format_offset >= entry_size) {
			entry.format = "";
		} else {
			const char *format_start = reinterpret_cast<const char *>(base + format_offset);
			const size_t max_format_len = entry_size - format_offset;
			const size_t format_len = strnlen(format_start, max_format_len);
			entry.format = std::string(format_start, format_len);
		}

		return entry_size;
	}

} // namespace CommonLowLevelTracingKit::decoder::source

namespace CommonLowLevelTracingKit::decoder {

	std::string MetaEntryInfo::typeToString(MetaEntryType t) {
		switch (t) {
		case MetaEntryType::Printf: return "printf";
		case MetaEntryType::Dump: return "dump";
		case MetaEntryType::Unknown:
		default: return "unknown";
		}
	}

	std::string MetaEntryInfo::argCharToTypeName(char c) {
		switch (c) {
		case 'c': return "uint8";
		case 'C': return "int8";
		case 'w': return "uint16";
		case 'W': return "int16";
		case 'i': return "uint32";
		case 'I': return "int32";
		case 'l': return "uint64";
		case 'L': return "int64";
		case 'q': return "uint128";
		case 'Q': return "int128";
		case 'f': return "float";
		case 'd': return "double";
		case 's': return "string";
		case 'p': return "pointer";
		case 'x': return "dump";
		default: return "unknown";
		}
	}

	std::vector<std::string> MetaEntryInfo::argumentTypeNames() const {
		std::vector<std::string> names;
		names.reserve(arg_types.size());
		for (char c : arg_types) { names.push_back(argCharToTypeName(c)); }
		return names;
	}

	std::string metaSourceTypeToString(MetaSourceType type) {
		switch (type) {
		case MetaSourceType::Tracebuffer: return "tracebuffer";
		case MetaSourceType::Snapshot: return "snapshot";
		case MetaSourceType::ElfSection: return "elf";
		case MetaSourceType::RawBlob: return "raw";
		default: return "unknown";
		}
	}

} // namespace CommonLowLevelTracingKit::decoder
