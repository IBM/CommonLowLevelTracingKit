// Copyright (c) 2024, International Business Machines
// SPDX-License-Identifier: BSD-2-Clause-Patent

#include "CommonLowLevelTracingKit/decoder/Meta.hpp"

#include <array>
#include <fstream>

#include "low_level/elf_reader.hpp"
#include "low_level/meta_parser.hpp"
#include "low_level/stack_section.hpp"
#include "low_level/tracebufferfile.hpp"

namespace fs = std::filesystem;
using namespace CommonLowLevelTracingKit::decoder;
using namespace CommonLowLevelTracingKit::decoder::source;

namespace {

	constexpr std::array<char, 16> TRACEBUFFER_MAGIC = {'?', '#', '$', '~', 't', 'r', 'a', 'c',
														'e', 'b', 'u', 'f', 'f', 'e', 'r', '\0'};
	constexpr std::string_view RAW_META_EXTENSION{".clltk_meta_data_raw"};

	bool hasTraceBufferMagic(const fs::path &path) {
		std::ifstream file(path, std::ios::binary);
		if (!file) { return false; }

		std::array<char, 16> magic{};
		file.read(magic.data(), magic.size());
		if (static_cast<size_t>(file.gcount()) < magic.size()) { return false; }

		return magic == TRACEBUFFER_MAGIC;
	}

	bool isTraceBufferFile(const fs::path &path) {
		if (!path.has_extension()) { return false; }
		const auto ext = path.extension();
		if (ext != ".clltk_trace" && ext != ".clltk_ktrace") { return false; }
		return hasTraceBufferMagic(path);
	}

	bool isRawMetaFile(const fs::path &path) {
		return path.string().ends_with(std::string(RAW_META_EXTENSION));
	}

	bool isSnapshotArchive(const fs::path &path) {
		if (!path.has_extension()) { return false; }
		return path.extension() == ".clltk";
	}

	MetaSourceInfo readTraceBufferMeta(const fs::path &path) {
		MetaSourceInfo info;
		info.path = path;
		info.source_type = MetaSourceType::Tracebuffer;

		try {
			TracebufferFile tbf(path.string());
			const auto &def = tbf.getDefinition();
			info.name = std::string(def.name());

			const auto stack_offset = StackSectionReader::getStackOffset(tbf.getFilePart());
			info.meta_size = StackSectionReader::getStackBodySize(tbf.getFilePart(), stack_offset);
			info.entries = StackSectionReader::parseMetaEntries(tbf.getFilePart(), stack_offset);

		} catch (const std::exception &e) { info.error = e.what(); } catch (...) {
			info.error = "unknown error reading tracebuffer";
		}

		return info;
	}

	MetaSourceInfo readRawMetaMeta(const fs::path &path) {
		MetaSourceInfo info;
		info.path = path;
		info.source_type = MetaSourceType::RawBlob;
		info.name = path.stem().string();

		try {
			std::ifstream file(path, std::ios::binary | std::ios::ate);
			if (!file) {
				info.error = "failed to open file";
				return info;
			}

			const auto size = file.tellg();
			if (size <= 0) {
				info.error = "empty file";
				return info;
			}

			std::vector<uint8_t> data(static_cast<size_t>(size));
			file.seekg(0);
			file.read(reinterpret_cast<char *>(data.data()), size);

			info.meta_size = data.size();
			info.entries = MetaParser::parse(std::span<const uint8_t>(data), 0);

		} catch (const std::exception &e) { info.error = e.what(); } catch (...) {
			info.error = "unknown error reading raw meta file";
		}

		return info;
	}

	void processFile(const fs::path &path, MetaSourceInfoCollection &results,
					 const std::function<bool(const std::string &)> &filter) {
		if (isTraceBufferFile(path)) {
			auto info = readTraceBufferMeta(path);
			if (!filter || filter(info.name)) { results.push_back(std::move(info)); }
		} else if (ElfReader::isElfFile(path) && ElfReader::hasClltkSections(path)) {
			auto elf_results = ElfReader::readAllMeta(path);
			for (auto &info : elf_results) {
				if (!filter || filter(info.name)) { results.push_back(std::move(info)); }
			}
		} else if (isRawMetaFile(path)) {
			auto info = readRawMetaMeta(path);
			if (!filter || filter(info.name)) { results.push_back(std::move(info)); }
		}
	}

} // namespace

MetaSourceInfoCollection CommonLowLevelTracingKit::decoder::getMetaInfo(
	const fs::path &path, bool recursive, const std::function<bool(const std::string &)> &filter) {
	MetaSourceInfoCollection results;

	if (!fs::exists(path)) { return results; }

	if (fs::is_directory(path)) {
		if (recursive) {
			for (const auto &entry : fs::recursive_directory_iterator(path)) {
				if (entry.is_regular_file()) { processFile(entry.path(), results, filter); }
			}
		} else {
			for (const auto &entry : fs::directory_iterator(path)) {
				if (entry.is_regular_file()) { processFile(entry.path(), results, filter); }
			}
		}
	} else if (fs::is_regular_file(path)) {
		processFile(path, results, filter);
	}

	return results;
}

bool CommonLowLevelTracingKit::decoder::hasMetaInfo(const fs::path &path) {
	if (!fs::exists(path) || !fs::is_regular_file(path)) { return false; }

	if (isTraceBufferFile(path)) { return true; }
	if (ElfReader::isElfFile(path) && ElfReader::hasClltkSections(path)) { return true; }
	if (isRawMetaFile(path)) { return true; }
	if (isSnapshotArchive(path)) { return true; }

	return false;
}

bool CommonLowLevelTracingKit::decoder::isElfWithClltk(const fs::path &path) {
	if (!fs::exists(path) || !fs::is_regular_file(path)) { return false; }
	return ElfReader::isElfFile(path) && ElfReader::hasClltkSections(path);
}

std::vector<std::string>
CommonLowLevelTracingKit::decoder::getElfClltkSections(const fs::path &path) {
	if (!fs::exists(path) || !fs::is_regular_file(path)) { return {}; }
	return ElfReader::getClltkSectionNames(path);
}
