// Copyright (c) 2024, International Business Machines
// SPDX-License-Identifier: BSD-2-Clause-Patent

#ifndef CLLTK_CMD_GZIP_OUTPUT_HPP
#define CLLTK_CMD_GZIP_OUTPUT_HPP

#include <cstddef>
#include <memory>
#include <string>
#include <zlib.h>

namespace CommonLowLevelTracingKit::cmd::interface
{

/**
 * @brief RAII wrapper for gzip-compressed output stream
 *
 * Provides printf-style interface for writing gzip-compressed data.
 * Can write to a file or to stdout.
 */
class GzipOutput
{
  public:
	/**
	 * @brief Open a gzip-compressed output file
	 * @param path File path (use "-" for stdout)
	 * @return GzipOutput instance, or nullptr on error
	 */
	static std::unique_ptr<GzipOutput> open(const std::string &path);

	~GzipOutput();

	// Non-copyable
	GzipOutput(const GzipOutput &) = delete;
	GzipOutput &operator=(const GzipOutput &) = delete;

	// Movable
	GzipOutput(GzipOutput &&other) noexcept;
	GzipOutput &operator=(GzipOutput &&other) noexcept;

	/**
	 * @brief Write formatted string (printf-style)
	 */
	template <typename... Args> int printf(const char *format, Args... args)
	{
		return gzprintf(m_gz, format, args...);
	}

	/**
	 * @brief Write a string
	 */
	int puts(const char *str);

	/**
	 * @brief Write raw bytes
	 */
	int write(const void *data, size_t size);

	/**
	 * @brief Flush the output
	 */
	int flush();

	/**
	 * @brief Check if using stdout
	 */
	bool is_stdout() const { return m_use_stdout; }

  private:
	explicit GzipOutput(gzFile gz, bool use_stdout) : m_gz(gz), m_use_stdout(use_stdout) {}

	gzFile m_gz{nullptr};
	bool m_use_stdout{false};
};

} // namespace CommonLowLevelTracingKit::cmd::interface

#endif // CLLTK_CMD_GZIP_OUTPUT_HPP
