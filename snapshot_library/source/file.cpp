// Copyright (c) 2024, International Business Machines
// SPDX-License-Identifier: BSD-2-Clause-Patent

#include "file.hpp"
#include <bits/struct_stat.h>
#include <cstdint>
#include <fcntl.h>
#include <filesystem>
#include <grp.h>
#include <iomanip>
#include <pwd.h>
#include <sstream>
#include <string>
#include <sys/mman.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <vector>

std::string CommonLowLevelTracingKit::snapshot::File::to_string(void) const
{
	std::ostringstream cout{};
	// Print file type and permissions
	cout << (S_ISDIR(status.st_mode) ? 'd' : '-');
	cout << ((status.st_mode & S_IRUSR) ? 'r' : '-');
	cout << ((status.st_mode & S_IWUSR) ? 'w' : '-');
	cout << ((status.st_mode & S_IXUSR) ? 'x' : '-');
	cout << ((status.st_mode & S_IRGRP) ? 'r' : '-');
	cout << ((status.st_mode & S_IWGRP) ? 'w' : '-');
	cout << ((status.st_mode & S_IXGRP) ? 'x' : '-');
	cout << ((status.st_mode & S_IROTH) ? 'r' : '-');
	cout << ((status.st_mode & S_IWOTH) ? 'w' : '-');
	cout << ((status.st_mode & S_IXOTH) ? 'x' : '-');

	// Get owner and group names
	struct passwd *pwd = getpwuid(status.st_uid);
	struct group *grp = getgrgid(status.st_gid);
	cout << ' ' << std::setw(10) << std::right << (pwd ? pwd->pw_name : "unknown");
	cout << '/' << std::setw(10) << std::left << (grp ? grp->gr_name : "unknown");

	// Print file size
	cout << " " << std::setw(10) << std::right << status.st_size;

	// Print modification time
	char timebuf[80];
	struct tm *tm = localtime(&status.st_mtime);
	strftime(timebuf, sizeof(timebuf), "%Y-%m-%d %H:%M", tm);
	cout << " " << timebuf;

	// Print file name
	cout << " " << filepath;

	return cout.str();
}

CommonLowLevelTracingKit::snapshot::RegularFile::RegularFile(
	const std::filesystem::directory_entry &entry, const std::filesystem::path &root_path)
{
	const std::string full_path = entry.path().string();
	filepath = std::filesystem::relative(full_path, root_path);
	stat(full_path.c_str(), &status);
	m_fd = open(full_path.c_str(), O_RDONLY);
	if (m_fd > 0) {
		content = mmap(nullptr, status.st_size, PROT_READ, MAP_PRIVATE, m_fd, 0);
	}
}
CommonLowLevelTracingKit::snapshot::RegularFile::~RegularFile()
{
	if (m_fd > 0) {
		close(m_fd);
		m_fd = 0;
	}
	if (content != nullptr && content != MAP_FAILED) {
		munmap((void *)content, status.st_size);
		content = nullptr;
	}
}

CommonLowLevelTracingKit::snapshot::VirtualFile::VirtualFile(
	const std::vector<std::string> &additional_tracepoints)
{
	const uint64_t time = time_s();

	filepath = "additional_tracepoints.json";
	m_file_content = to_file_content(additional_tracepoints, time_ns());
	content = m_file_content.c_str();
	status.st_size = m_file_content.size();
	status.st_ino = 0xAFFE;
	status.st_mode = S_IFREG | S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
	status.st_nlink = 1;
	status.st_uid = getuid();
	status.st_gid = getgid();
	status.st_rdev = 0;
	status.st_blksize = 512;
	status.st_blocks = 1;
	status.st_atime = time;
	status.st_mtime = time;
	status.st_ctime = time;
}

uint64_t CommonLowLevelTracingKit::snapshot::VirtualFile::time_s()
{
	struct timespec t = {0, 0};
	timespec_get(&t, TIME_UTC);
	return (uint64_t)t.tv_sec;
}
uint64_t CommonLowLevelTracingKit::snapshot::VirtualFile::time_ns()
{
	struct timespec t = {0, 0};
	timespec_get(&t, TIME_UTC);
	return (uint64_t)t.tv_sec * 1000lu * 1000lu * 1000lu + (uint64_t)t.tv_nsec;
}

std::string CommonLowLevelTracingKit::snapshot::VirtualFile::to_file_content(
	const std::vector<std::string> &additional_tracepoints, const uint64_t ns)
{
	std::stringstream file_content_buffer;
	file_content_buffer << "[";
	bool frist = true;
	for (const auto &formatted : additional_tracepoints) {
		if (!frist)
			file_content_buffer << ",";
		frist = false;
		file_content_buffer << "{\"timestamp\":" << ns << ",\"formatted\":\"" << formatted << "\"}";
	}
	file_content_buffer << "]";
	return file_content_buffer.str();
}

std::vector<std::unique_ptr<CommonLowLevelTracingKit::snapshot::File>>
CommonLowLevelTracingKit::snapshot::getAllFiles(const std::filesystem::path &root_path)
{
	std::vector<std::unique_ptr<File>> files{};

	if (std::filesystem::is_empty(root_path)) {
		return files;
	}

	const auto diriter = std::filesystem::recursive_directory_iterator(root_path);
	if (diriter == std::filesystem::end(diriter)) {
		// could not open
		return files;
	}

	for (const auto &entry : diriter) {
		if (entry.is_regular_file()) {
			files.push_back(std::make_unique<RegularFile>(entry, root_path));
		}
	}
	return files;
}
