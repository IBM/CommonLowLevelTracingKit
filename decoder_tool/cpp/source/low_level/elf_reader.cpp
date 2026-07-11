// Copyright (c) 2024, International Business Machines
// SPDX-License-Identifier: BSD-2-Clause-Patent

#include "elf_reader.hpp"
#include "file.hpp"
#include "meta_parser.hpp"

#include <algorithm>
#include <bit>
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

		// an ELF file declares its byte order in the identification bytes;
		// foreign means the opposite of this host's order
		bool isForeignElf(const std::vector<uint8_t> &data) {
			if (data.size() < EI_NIDENT) { return false; }
			constexpr uint8_t host_order =
				(std::endian::native == std::endian::little) ? ELFDATA2LSB : ELFDATA2MSB;
			const uint8_t file_order = data[EI_DATA];
			return (file_order == ELFDATA2LSB || file_order == ELFDATA2MSB) &&
				   (file_order != host_order);
		}

		template <typename T> T swapped(T value, bool foreign) {
			return foreign ? internal::byteswapValue(value) : value;
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

			const bool foreign = isForeignElf(data);
			const auto *ehdr = reinterpret_cast<const Elf64_Ehdr *>(data.data());
			const uint64_t e_shoff = swapped(ehdr->e_shoff, foreign);
			const uint16_t e_shnum = swapped(ehdr->e_shnum, foreign);
			const uint16_t e_shstrndx = swapped(ehdr->e_shstrndx, foreign);
			if (e_shoff == 0 || e_shnum == 0) { return sections; }
			if (e_shoff + e_shnum * sizeof(Elf64_Shdr) > data.size()) { return sections; }

			if (e_shstrndx >= e_shnum) { return sections; }

			const auto *shdr_base = reinterpret_cast<const Elf64_Shdr *>(data.data() + e_shoff);
			const uint64_t shstrtab_offset = swapped(shdr_base[e_shstrndx].sh_offset, foreign);

			for (uint16_t i = 0; i < e_shnum; ++i) {
				const auto &shdr = shdr_base[i];

				ElfSectionInfo info;
				info.name = getSectionName(data, shstrtab_offset, swapped(shdr.sh_name, foreign));
				info.offset = swapped(shdr.sh_offset, foreign);
				info.size = swapped(shdr.sh_size, foreign);
				info.addr = swapped(shdr.sh_addr, foreign);
				info.type = swapped(shdr.sh_type, foreign);

				sections.push_back(std::move(info));
			}

			return sections;
		}

		std::vector<ElfSectionInfo> parseSections32(const std::vector<uint8_t> &data) {
			std::vector<ElfSectionInfo> sections;

			if (data.size() < sizeof(Elf32_Ehdr)) { return sections; }

			const bool foreign = isForeignElf(data);
			const auto *ehdr = reinterpret_cast<const Elf32_Ehdr *>(data.data());
			const uint32_t e_shoff = swapped(ehdr->e_shoff, foreign);
			const uint16_t e_shnum = swapped(ehdr->e_shnum, foreign);
			const uint16_t e_shstrndx = swapped(ehdr->e_shstrndx, foreign);
			if (e_shoff == 0 || e_shnum == 0) { return sections; }
			if (e_shoff + e_shnum * sizeof(Elf32_Shdr) > data.size()) { return sections; }
			if (e_shstrndx >= e_shnum) { return sections; }

			const auto *shdr_base = reinterpret_cast<const Elf32_Shdr *>(data.data() + e_shoff);
			const uint32_t shstrtab_offset = swapped(shdr_base[e_shstrndx].sh_offset, foreign);

			for (uint16_t i = 0; i < e_shnum; ++i) {
				const auto &shdr = shdr_base[i];

				ElfSectionInfo info;
				info.name = getSectionName(data, shstrtab_offset, swapped(shdr.sh_name, foreign));
				info.offset = swapped(shdr.sh_offset, foreign);
				info.size = swapped(shdr.sh_size, foreign);
				info.addr = swapped(shdr.sh_addr, foreign);
				info.type = swapped(shdr.sh_type, foreign);

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

		// parse one self-describing meta entry (magic + size prefix) located
		// at a file offset and append the result
		void appendMetaEntryAt(const std::vector<uint8_t> &data, uint64_t entry_offset,
							   MetaEntryInfoCollection &entries, bool foreign) {
			if (entry_offset + MetaParser::MIN_ENTRY_SIZE > data.size()) { return; }

			uint32_t entry_size = 0;
			std::memcpy(&entry_size, data.data() + entry_offset + MetaParser::OFFSET_SIZE,
						sizeof(entry_size));
			entry_size = swapped(entry_size, foreign);
			if (entry_size < MetaParser::MIN_ENTRY_SIZE ||
				entry_offset + entry_size > data.size()) {
				return;
			}

			const std::span<const uint8_t> entry_data(data.data() + entry_offset, entry_size);
			auto parsed = MetaParser::parse(entry_data, entry_offset, foreign);
			entries.insert(entries.end(), std::make_move_iterator(parsed.begin()),
						   std::make_move_iterator(parsed.end()));
		}

		bool isRelocatable(const std::vector<uint8_t> &data) {
			if (data.size() < sizeof(Elf64_Ehdr)) { return false; }
			// e_type sits at the same offset for ELFCLASS32 and ELFCLASS64
			uint16_t e_type = 0;
			std::memcpy(&e_type, data.data() + offsetof(Elf64_Ehdr, e_type), sizeof(e_type));
			return swapped(e_type, isForeignElf(data)) == ET_REL;
		}

		// In relocatable objects (.o) the pointer slots are zero; the meta
		// locations live in the .rela<section> relocation records instead:
		// each relocation names a symbol (whose section and value give the
		// target location) plus an addend. Only entries at even slots are
		// meta pointers; odd slots point at the runtime offset caches and
		// are skipped. 64-bit RELA only, which covers all supported targets.
		MetaEntryInfoCollection
		parseRelocatedPointerSection(const std::vector<uint8_t> &data,
									 const std::vector<ElfSectionInfo> &sections,
									 const ElfSectionInfo &ptr_section, size_t pointer_size) {
			MetaEntryInfoCollection entries;
			if (!is64Bit(data)) { return entries; }

			const ElfSectionInfo *symtab = nullptr;
			const ElfSectionInfo *rela = nullptr;
			const std::string rela_name = ".rela" + ptr_section.name;
			for (const auto &section : sections) {
				if (section.type == SHT_SYMTAB) { symtab = &section; }
				if (section.type == SHT_RELA && section.name == rela_name) { rela = &section; }
			}
			if (symtab == nullptr || rela == nullptr) { return entries; }
			if (rela->offset + rela->size > data.size()) { return entries; }

			const bool foreign = isForeignElf(data);
			std::vector<uint64_t> seen;
			const size_t entry_stride = 2 * pointer_size;
			const size_t count = rela->size / sizeof(Elf64_Rela);
			for (size_t i = 0; i < count; ++i) {
				Elf64_Rela relocation = {};
				std::memcpy(&relocation, data.data() + rela->offset + i * sizeof(relocation),
							sizeof(relocation));
				relocation.r_offset = swapped(relocation.r_offset, foreign);
				relocation.r_info = swapped(relocation.r_info, foreign);
				relocation.r_addend = swapped(relocation.r_addend, foreign);
				if (relocation.r_offset % entry_stride != 0) { continue; } // offset cache slot

				const uint64_t sym_index = ELF64_R_SYM(relocation.r_info);
				const uint64_t sym_offset = symtab->offset + sym_index * sizeof(Elf64_Sym);
				if (sym_offset + sizeof(Elf64_Sym) > data.size() ||
					(sym_index + 1) * sizeof(Elf64_Sym) > symtab->size) {
					continue;
				}
				Elf64_Sym symbol = {};
				std::memcpy(&symbol, data.data() + sym_offset, sizeof(symbol));
				symbol.st_shndx = swapped(symbol.st_shndx, foreign);
				symbol.st_value = swapped(symbol.st_value, foreign);
				if (symbol.st_shndx == SHN_UNDEF || symbol.st_shndx >= sections.size()) {
					continue;
				}

				const auto &target = sections[symbol.st_shndx];
				const uint64_t entry_offset =
					target.offset + symbol.st_value + (uint64_t)relocation.r_addend;
				if (std::find(seen.begin(), seen.end(), entry_offset) != seen.end()) { continue; }
				seen.push_back(entry_offset);
				appendMetaEntryAt(data, entry_offset, entries, foreign);
			}

			return entries;
		}

		// Parse a pointer-layout discovery section: every entry is a pointer
		// pair {meta address, offset-cache address}. The meta address points
		// into some allocated section (e.g. .rodata); the offset cache is
		// runtime-only state and ignored here. Resolve each meta address
		// through the section table to a file offset and parse the
		// self-describing meta entry found there. Duplicate addresses (e.g.
		// from inlined call sites) are skipped.
		MetaEntryInfoCollection parsePointerSection(const std::vector<uint8_t> &data,
													const std::vector<ElfSectionInfo> &sections,
													const ElfSectionInfo &ptr_section,
													size_t pointer_size) {
			MetaEntryInfoCollection entries;

			const bool foreign = isForeignElf(data);
			std::vector<uint64_t> seen;
			const size_t entry_stride = 2 * pointer_size;
			const size_t count = ptr_section.size / entry_stride;
			for (size_t i = 0; i < count; ++i) {
				const size_t ptr_offset = ptr_section.offset + i * entry_stride;
				if (ptr_offset + pointer_size > data.size()) { break; }

				uint64_t address = 0;
				if (pointer_size == sizeof(uint64_t)) {
					uint64_t raw = 0;
					std::memcpy(&raw, data.data() + ptr_offset, sizeof(raw));
					address = swapped(raw, foreign);
				} else {
					uint32_t raw = 0;
					std::memcpy(&raw, data.data() + ptr_offset, sizeof(raw));
					address = swapped(raw, foreign);
				}
				if (address == 0) { continue; }
				if (std::find(seen.begin(), seen.end(), address) != seen.end()) { continue; }
				seen.push_back(address);

				for (const auto &section : sections) {
					if (section.addr == 0 || section.size == 0) { continue; }
					if (address < section.addr || address >= section.addr + section.size) {
						continue;
					}
					appendMetaEntryAt(data, section.offset + (address - section.addr), entries,
									  foreign);
					break;
				}
			}

			return entries;
		}

		MetaEntryInfoCollection parseAnyPointerSection(const std::vector<uint8_t> &data,
													   const std::vector<ElfSectionInfo> &sections,
													   const ElfSectionInfo &ptr_section,
													   size_t pointer_size) {
			if (isRelocatable(data)) {
				return parseRelocatedPointerSection(data, sections, ptr_section, pointer_size);
			}
			return parsePointerSection(data, sections, ptr_section, pointer_size);
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
				return parseAnyPointerSection(data, sections, section, pointerSize(data));
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
				info.entries = parseAnyPointerSection(data, sections, section, pointerSize(data));
				if (info.entries.empty() && section.size >= pointerSize(data)) {
					info.error = "no resolvable meta pointers";
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
