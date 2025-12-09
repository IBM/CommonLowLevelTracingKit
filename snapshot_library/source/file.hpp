// Copyright (c) 2024, International Business Machines
// SPDX-License-Identifier: BSD-2-Clause-Patent

#ifndef _CLLTK_SNAPSHOT_FILE_HEADER_HPP_
#define _CLLTK_SNAPSHOT_FILE_HEADER_HPP_

#include <bit>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <stddef.h>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>
#include <vector>

namespace CommonLowLevelTracingKit::snapshot
{

struct File {
	template <typename Type> const Type *at(off_t offset) const
	{
		const intptr_t base = std::bit_cast<intptr_t>(content);
		return std::bit_cast<const Type *>(base + offset);
	}

	std::string to_string(void) const;
	virtual ~File() = default;

	const std::string &getFilepath(void) const { return filepath; }
	const struct stat &getFilStatus(void) const { return status; }
	size_t getFilSize(void) const { return status.st_size; }

  protected:
	std::string filepath;
	struct stat status;
	const void *content{};
	File() = default;
};

struct RegularFile : public File {
	RegularFile(const std::filesystem::directory_entry &entry,
				const std::filesystem::path &root_path);

	~RegularFile() override;

  private:
	int m_fd{};
};

struct VirtualFile : public File {
	VirtualFile(const std::vector<std::string> &additional_tracepoints);

  private:
	std::string m_file_content{};
	static uint64_t time_s();
	static uint64_t time_ns();

	static std::string to_file_content(const std::vector<std::string> &additional_tracepoints,
									   const uint64_t ns);
};

std::vector<std::unique_ptr<File>> getAllFiles(const std::filesystem::path &root_path);

} // namespace CommonLowLevelTracingKit::snapshot

#endif // !_CLLTK_SNAPSHOT_FILE_HEADER_HPP_
