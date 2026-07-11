// Copyright (c) 2024, International Business Machines
// SPDX-License-Identifier: BSD-2-Clause-Patent

#ifndef DECODER_TOOL_SOURCE_ELF_READER_HEADER
#define DECODER_TOOL_SOURCE_ELF_READER_HEADER

#include "CommonLowLevelTracingKit/decoder/Meta.hpp"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace CommonLowLevelTracingKit::decoder::source {

	struct ElfSectionInfo {
		std::string name;
		uint64_t offset;
		uint64_t size;
		uint64_t addr;
		uint32_t type;
	};

	class ElfReader final {
	  public:
		static bool isElfFile(const std::filesystem::path &path);
		static bool hasClltkSections(const std::filesystem::path &path);
		static std::vector<std::string> getClltkSectionNames(const std::filesystem::path &path);
		static std::vector<ElfSectionInfo> getSections(const std::filesystem::path &path);
		static MetaEntryInfoCollection readMetaFromSection(const std::filesystem::path &path,
														   const std::string &section_name);
		static MetaSourceInfoCollection readAllMeta(const std::filesystem::path &path);
		static std::string extractTracebufferName(const std::string &section_name);

		static constexpr const char *SECTION_PREFIX = "_clltk_";
		/// legacy layout: meta entries stored inline in the section
		static constexpr const char *SECTION_SUFFIX = "_meta";
		/// current layout: the section stores pointers to meta entries that
		/// live in regular data sections (see _CLLTK_EMIT_META_PTR)
		static constexpr const char *SECTION_PTR_SUFFIX = "_metaptr";

	  private:
		ElfReader() = delete;
		static constexpr uint8_t ELF_MAGIC[4] = {0x7f, 'E', 'L', 'F'};
	};

} // namespace CommonLowLevelTracingKit::decoder::source

#endif
