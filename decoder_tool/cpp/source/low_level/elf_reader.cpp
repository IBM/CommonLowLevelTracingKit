// Copyright (c) 2024, International Business Machines
// SPDX-License-Identifier: BSD-2-Clause-Patent

#include "elf_reader.hpp"
#include "meta_parser.hpp"

#include <algorithm>
#include <cstring>
#include <elf.h>
#include <fstream>
#include <span>

namespace CommonLowLevelTracingKit::decoder::source {

	namespace {
		std::vector<uint8_t> readFile(const std::filesystem::path &path) {
			std::ifstream file(path, std::ios::binary | std::ios::ate);
			if (!file) { return {}; }

			const auto size = file.tellg();
			if (size <= 0) { return {}; }

			std::vector<uint8_t> buffer(static_cast<size_t>(size));
			file.seekg(0);
			file.read(reinterpret_cast<char *>(buffer.data()), size);
			return buffer;
		}

		bool is64Bit(const std::vector<uint8_t> &data) {
			if (data.size() < EI_NIDENT) { return false; }
			return data[EI_CLASS] == ELFCLASS64;
		}

		std::string getSectionName(const std::vector<uint8_t> &data, uint64_t strtab_offset,
								   uint32_t name_index) {
			if (strtab_offset + name_index >= data.size()) { return ""; }

			const char *start =
				reinterpret_cast<const char *>(data.data() + strtab_offset + name_index);
			const size_t max_len = data.size() - (strtab_offset + name_index);
			const size_t len = strnlen(start, max_len);
			return std::string(start, len);
		}

		std::vector<ElfSectionInfo> parseSections64(const std::vector<uint8_t> &data) {
			std::vector<ElfSectionInfo> sections;

			if (data.size() < sizeof(Elf64_Ehdr)) { return sections; }

			const auto *ehdr = reinterpret_cast<const Elf64_Ehdr *>(data.data());
			if (ehdr->e_shoff == 0 || ehdr->e_shnum == 0) { return sections; }
			if (ehdr->e_shoff + ehdr->e_shnum * sizeof(Elf64_Shdr) > data.size()) {
				return sections;
			}

			if (ehdr->e_shstrndx >= ehdr->e_shnum) { return sections; }

			const auto *shdr_base =
				reinterpret_cast<const Elf64_Shdr *>(data.data() + ehdr->e_shoff);
			const auto &shstrtab_hdr = shdr_base[ehdr->e_shstrndx];

			for (uint16_t i = 0; i < ehdr->e_shnum; ++i) {
				const auto &shdr = shdr_base[i];

				ElfSectionInfo info;
				info.name = getSectionName(data, shstrtab_hdr.sh_offset, shdr.sh_name);
				info.offset = shdr.sh_offset;
				info.size = shdr.sh_size;
				info.addr = shdr.sh_addr;
				info.type = shdr.sh_type;

				sections.push_back(std::move(info));
			}

			return sections;
		}

		std::vector<ElfSectionInfo> parseSections32(const std::vector<uint8_t> &data) {
			std::vector<ElfSectionInfo> sections;

			if (data.size() < sizeof(Elf32_Ehdr)) { return sections; }

			const auto *ehdr = reinterpret_cast<const Elf32_Ehdr *>(data.data());
			if (ehdr->e_shoff == 0 || ehdr->e_shnum == 0) { return sections; }
			if (ehdr->e_shoff + ehdr->e_shnum * sizeof(Elf32_Shdr) > data.size()) {
				return sections;
			}
			if (ehdr->e_shstrndx >= ehdr->e_shnum) { return sections; }

			const auto *shdr_base =
				reinterpret_cast<const Elf32_Shdr *>(data.data() + ehdr->e_shoff);
			const auto &shstrtab_hdr = shdr_base[ehdr->e_shstrndx];

			for (uint16_t i = 0; i < ehdr->e_shnum; ++i) {
				const auto &shdr = shdr_base[i];

				ElfSectionInfo info;
				info.name = getSectionName(data, shstrtab_hdr.sh_offset, shdr.sh_name);
				info.offset = shdr.sh_offset;
				info.size = shdr.sh_size;
				info.addr = shdr.sh_addr;
				info.type = shdr.sh_type;

				sections.push_back(std::move(info));
			}

			return sections;
		}

		bool hasPrefixAndSuffix(const std::string &name, const char *prefix, const char *suffix) {
			const size_t prefix_len = std::strlen(prefix);
			const size_t suffix_len = std::strlen(suffix);

			if (name.size() < prefix_len + suffix_len + 1) { return false; }
			if (name.compare(0, prefix_len, prefix) != 0) { return false; }
			if (name.compare(name.size() - suffix_len, suffix_len, suffix) != 0) { return false; }
			return true;
		}

		bool isClltkMetaPtrSection(const std::string &name) {
			return hasPrefixAndSuffix(name, ElfReader::SECTION_PREFIX,
									  ElfReader::SECTION_PTR_SUFFIX);
		}

		bool isClltkMetaSection(const std::string &name) {
			return !isClltkMetaPtrSection(name) &&
				   hasPrefixAndSuffix(name, ElfReader::SECTION_PREFIX, ElfReader::SECTION_SUFFIX);
		}

		bool isAnyClltkMetaSection(const std::string &name) {
			return isClltkMetaSection(name) || isClltkMetaPtrSection(name);
		}

		// Parse a pointer-layout discovery section: every entry is a virtual
		// address of a meta entry located in some allocated section (e.g.
		// .rodata). Resolve each address through the section table to a file
		// offset and parse the self-describing meta entry found there.
		// Duplicate addresses (e.g. from inlined call sites) are skipped.
		MetaEntryInfoCollection parsePointerSection(const std::vector<uint8_t> &data,
													const std::vector<ElfSectionInfo> &sections,
													const ElfSectionInfo &ptr_section,
													size_t pointer_size) {
			MetaEntryInfoCollection entries;

			std::vector<uint64_t> seen;
			const size_t count = ptr_section.size / pointer_size;
			for (size_t i = 0; i < count; ++i) {
				const size_t ptr_offset = ptr_section.offset + i * pointer_size;
				if (ptr_offset + pointer_size > data.size()) { break; }

				uint64_t address = 0;
				std::memcpy(&address, data.data() + ptr_offset, pointer_size);
				if (address == 0) { continue; } // unresolved (relocatable object)
				if (std::find(seen.begin(), seen.end(), address) != seen.end()) { continue; }
				seen.push_back(address);

				for (const auto &section : sections) {
					if (section.addr == 0 || section.size == 0) { continue; }
					if (address < section.addr || address >= section.addr + section.size) {
						continue;
					}
					const uint64_t entry_offset = section.offset + (address - section.addr);
					if (entry_offset + MetaParser::MIN_ENTRY_SIZE > data.size()) { break; }

					uint32_t entry_size = 0;
					std::memcpy(&entry_size, data.data() + entry_offset + MetaParser::OFFSET_SIZE,
								sizeof(entry_size));
					if (entry_size < MetaParser::MIN_ENTRY_SIZE ||
						entry_offset + entry_size > data.size()) {
						break;
					}

					const std::span<const uint8_t> entry_data(data.data() + entry_offset,
															  entry_size);
					auto parsed = MetaParser::parse(entry_data, entry_offset);
					entries.insert(entries.end(), std::make_move_iterator(parsed.begin()),
								   std::make_move_iterator(parsed.end()));
					break;
				}
			}

			return entries;
		}

		size_t pointerSize(const std::vector<uint8_t> &data) {
			return is64Bit(data) ? sizeof(uint64_t) : sizeof(uint32_t);
		}

	} // namespace

	bool ElfReader::isElfFile(const std::filesystem::path &path) {
		std::ifstream file(path, std::ios::binary);
		if (!file) { return false; }

		uint8_t magic[4];
		file.read(reinterpret_cast<char *>(magic), sizeof(magic));
		if (!file || file.gcount() != sizeof(magic)) { return false; }

		return std::memcmp(magic, ELF_MAGIC, sizeof(ELF_MAGIC)) == 0;
	}

	bool ElfReader::hasClltkSections(const std::filesystem::path &path) {
		const auto sections = getSections(path);
		return std::any_of(sections.begin(), sections.end(),
						   [](const auto &s) { return isAnyClltkMetaSection(s.name); });
	}

	std::vector<std::string> ElfReader::getClltkSectionNames(const std::filesystem::path &path) {
		std::vector<std::string> names;
		const auto sections = getSections(path);

		for (const auto &section : sections) {
			if (isAnyClltkMetaSection(section.name)) { names.push_back(section.name); }
		}

		return names;
	}

	std::vector<ElfSectionInfo> ElfReader::getSections(const std::filesystem::path &path) {
		const auto data = readFile(path);
		if (data.size() < EI_NIDENT) { return {}; }
		if (std::memcmp(data.data(), ELF_MAGIC, sizeof(ELF_MAGIC)) != 0) { return {}; }

		if (is64Bit(data)) { return parseSections64(data); }
		return parseSections32(data);
	}

	std::string ElfReader::extractTracebufferName(const std::string &section_name) {
		const size_t prefix_len = std::strlen(SECTION_PREFIX);

		if (isClltkMetaPtrSection(section_name)) {
			const size_t suffix_len = std::strlen(SECTION_PTR_SUFFIX);
			return section_name.substr(prefix_len, section_name.size() - prefix_len - suffix_len);
		}
		if (isClltkMetaSection(section_name)) {
			const size_t suffix_len = std::strlen(SECTION_SUFFIX);
			return section_name.substr(prefix_len, section_name.size() - prefix_len - suffix_len);
		}
		return "";
	}

	MetaEntryInfoCollection ElfReader::readMetaFromSection(const std::filesystem::path &path,
														   const std::string &section_name) {
		const auto data = readFile(path);
		const auto sections = is64Bit(data) ? parseSections64(data) : parseSections32(data);

		for (const auto &section : sections) {
			if (section.name != section_name) { continue; }
			if (section.offset + section.size > data.size()) { continue; }

			if (isClltkMetaPtrSection(section.name)) {
				return parsePointerSection(data, sections, section, pointerSize(data));
			}
			const std::span<const uint8_t> section_data(data.data() + section.offset, section.size);
			return MetaParser::parse(section_data, section.offset);
		}

		return {};
	}

	MetaSourceInfoCollection ElfReader::readAllMeta(const std::filesystem::path &path) {
		MetaSourceInfoCollection results;

		const auto data = readFile(path);
		if (data.size() < EI_NIDENT) { return results; }
		if (std::memcmp(data.data(), ELF_MAGIC, sizeof(ELF_MAGIC)) != 0) { return results; }

		const auto sections = is64Bit(data) ? parseSections64(data) : parseSections32(data);

		for (const auto &section : sections) {
			if (!isAnyClltkMetaSection(section.name)) { continue; }

			MetaSourceInfo info;
			info.name = extractTracebufferName(section.name);
			info.path = path;
			info.source_type = MetaSourceType::ElfSection;
			info.meta_size = section.size;

			if (section.offset + section.size > data.size()) {
				info.error = "Section extends beyond file";
				results.push_back(std::move(info));
				continue;
			}

			if (isClltkMetaPtrSection(section.name)) {
				info.entries = parsePointerSection(data, sections, section, pointerSize(data));
				if (info.entries.empty() && section.size >= pointerSize(data)) {
					info.error = "no resolvable meta pointers (relocatable object?)";
				}
			} else {
				const std::span<const uint8_t> section_data(data.data() + section.offset,
															section.size);
				info.entries = MetaParser::parse(section_data, section.offset);
			}

			results.push_back(std::move(info));
		}

		return results;
	}

} // namespace CommonLowLevelTracingKit::decoder::source
