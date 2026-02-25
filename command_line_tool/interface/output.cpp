// Copyright (c) 2024, International Business Machines
// SPDX-License-Identifier: BSD-2-Clause-Patent

#include "commands/output.hpp"

#include <vector>

namespace CommonLowLevelTracingKit::cmd::interface
{

int FileOutput::printf(const char *format, ...)
{
	va_list args;
	va_start(args, format);
	int result = vfprintf(m_file, format, args);
	va_end(args);
	return result;
}

void FileOutput::flush()
{
	fflush(m_file);
}

int GzipFileOutput::printf(const char *format, ...)
{
	va_list args;
	va_start(args, format);
	// gzprintf doesn't support va_list, so we need to format first
	char buffer[8192];
	int len = vsnprintf(buffer, sizeof(buffer), format, args);
	va_end(args);
	if (len <= 0) {
		return len;
	}
	size_t write_len = static_cast<size_t>(len);
	if (write_len < sizeof(buffer)) {
		// Common fast path: output fits in the stack buffer
		return m_gz->write(buffer, write_len);
	}
	// Output exceeded the stack buffer -- allocate and re-format
	std::vector<char> heap_buf(write_len + 1);
	va_list args2;
	va_start(args2, format);
	vsnprintf(heap_buf.data(), heap_buf.size(), format, args2);
	va_end(args2);
	return m_gz->write(heap_buf.data(), write_len);
}

void GzipFileOutput::flush()
{
	m_gz->flush();
}

std::unique_ptr<Output> create_output(const std::string &path, bool compress, FILE **raw_file)
{
	bool use_stdout = path.empty() || path == "-";

	if (compress) {
		auto gz = GzipOutput::open(use_stdout ? "-" : path);
		if (!gz) {
			return nullptr;
		}
		return std::make_unique<GzipFileOutput>(std::move(gz));
	}

	FILE *f = use_stdout ? stdout : std::fopen(path.c_str(), "w+");
	if (!use_stdout && !f) {
		return nullptr;
	}
	if (raw_file) {
		*raw_file = f;
	}
	return std::make_unique<FileOutput>(f, use_stdout);
}

} // namespace CommonLowLevelTracingKit::cmd::interface
