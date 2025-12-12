// Copyright (c) 2024, International Business Machines
// SPDX-License-Identifier: BSD-2-Clause-Patent

#ifndef DECODER_TOOL_META_HEADER
#define DECODER_TOOL_META_HEADER

#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "Common.hpp"

namespace CommonLowLevelTracingKit::decoder {

	enum class MetaEntryType : uint8_t { Unknown = 0, Printf = 1, Dump = 2 };

	struct EXPORT MetaEntryInfo {
		uint64_t offset;
		uint32_t size;
		MetaEntryType type;
		uint32_t line;
		uint8_t arg_count;
		std::string arg_types;
		std::string file;
		std::string format;

		std::vector<std::string> argumentTypeNames() const;
		static std::string typeToString(MetaEntryType t);
		static std::string argCharToTypeName(char c);
	};

	using MetaEntryInfoCollection = std::vector<MetaEntryInfo>;

	enum class MetaSourceType { Tracebuffer, Snapshot, ElfSection, RawBlob };

	struct EXPORT MetaSourceInfo {
		std::string name;
		std::filesystem::path path;
		MetaSourceType source_type;
		uint64_t meta_size;
		MetaEntryInfoCollection entries;
		std::optional<std::string> error;

		bool valid() const noexcept { return !error.has_value(); }
	};

	using MetaSourceInfoCollection = std::vector<MetaSourceInfo>;

	EXPORT std::string metaSourceTypeToString(MetaSourceType type);

	EXPORT MetaSourceInfoCollection
	getMetaInfo(const std::filesystem::path &path, bool recursive = false,
				const std::function<bool(const std::string &)> &filter = {});

	EXPORT bool hasMetaInfo(const std::filesystem::path &path);

	EXPORT bool isElfWithClltk(const std::filesystem::path &path);

	EXPORT std::vector<std::string> getElfClltkSections(const std::filesystem::path &path);

} // namespace CommonLowLevelTracingKit::decoder

#endif
