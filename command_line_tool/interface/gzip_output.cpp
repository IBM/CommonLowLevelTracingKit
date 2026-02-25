// Copyright (c) 2024, International Business Machines
// SPDX-License-Identifier: BSD-2-Clause-Patent

#include "commands/gzip_output.hpp"

#include <unistd.h>

namespace CommonLowLevelTracingKit::cmd::interface
{

std::unique_ptr<GzipOutput> GzipOutput::open(const std::string &path)
{
	gzFile gz = nullptr;
	bool use_stdout = (path.empty() || path == "-");

	if (use_stdout) {
		// dup() stdout's fd so that gzclose() later closes only the
		// duplicate, leaving the real stdout (fd 1) intact.
		int fd = dup(fileno(stdout));
		if (fd < 0) {
			return nullptr;
		}
		gz = gzdopen(fd, "wb");
		if (gz == nullptr) {
			close(fd);
			return nullptr;
		}
	} else {
		gz = gzopen(path.c_str(), "wb");
	}

	if (gz == nullptr) {
		return nullptr;
	}

	return std::unique_ptr<GzipOutput>(new GzipOutput(gz, use_stdout));
}

GzipOutput::~GzipOutput()
{
	if (m_gz != nullptr) {
		gzclose(m_gz);
		m_gz = nullptr;
	}
}

GzipOutput::GzipOutput(GzipOutput &&other) noexcept
	: m_gz(other.m_gz), m_use_stdout(other.m_use_stdout)
{
	other.m_gz = nullptr;
}

GzipOutput &GzipOutput::operator=(GzipOutput &&other) noexcept
{
	if (this != &other) {
		if (m_gz != nullptr) {
			gzclose(m_gz);
		}
		m_gz = other.m_gz;
		m_use_stdout = other.m_use_stdout;
		other.m_gz = nullptr;
	}
	return *this;
}

int GzipOutput::puts(const char *str)
{
	return gzputs(m_gz, str);
}

int GzipOutput::write(const void *data, size_t size)
{
	return gzwrite(m_gz, data, static_cast<unsigned int>(size));
}

int GzipOutput::flush()
{
	return gzflush(m_gz, Z_SYNC_FLUSH);
}

} // namespace CommonLowLevelTracingKit::cmd::interface
