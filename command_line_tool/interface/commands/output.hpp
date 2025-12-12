// Copyright (c) 2024, International Business Machines
// SPDX-License-Identifier: BSD-2-Clause-Patent

#ifndef CLLTK_CMD_OUTPUT_HPP
#define CLLTK_CMD_OUTPUT_HPP

#include "gzip_output.hpp"
#include <cstdio>
#include <memory>
#include <string>

namespace CommonLowLevelTracingKit::cmd::interface
{

/**
 * @brief Output abstraction for compressed and uncompressed output
 *
 * Provides a unified interface for writing formatted output to either
 * a FILE* or a gzip-compressed stream.
 */
class Output
{
  public:
	virtual ~Output() = default;
	virtual int printf(const char *format, ...) = 0;
	virtual void flush() = 0;
	virtual bool is_stdout() const = 0;
};

/**
 * @brief Output implementation for uncompressed FILE* output
 */
class FileOutput : public Output
{
  public:
	explicit FileOutput(FILE *f, bool is_stdout) : m_file(f), m_is_stdout(is_stdout) {}

	int printf(const char *format, ...) override;
	void flush() override;
	bool is_stdout() const override { return m_is_stdout; }

	FILE *file() const { return m_file; }

  private:
	FILE *m_file;
	bool m_is_stdout;
};

/**
 * @brief Output implementation for gzip-compressed output
 */
class GzipFileOutput : public Output
{
  public:
	explicit GzipFileOutput(std::unique_ptr<GzipOutput> gz) : m_gz(std::move(gz)) {}

	int printf(const char *format, ...) override;
	void flush() override;
	bool is_stdout() const override { return m_gz->is_stdout(); }

  private:
	std::unique_ptr<GzipOutput> m_gz;
};

/**
 * @brief Create an Output instance for the given path and compression setting
 *
 * @param path Output path ("" or "-" for stdout)
 * @param compress Whether to use gzip compression
 * @param raw_file [out] If non-null and not compressing, set to the opened FILE*
 * @return Output instance, or nullptr on error
 */
std::unique_ptr<Output> create_output(const std::string &path, bool compress,
									  FILE **raw_file = nullptr);

} // namespace CommonLowLevelTracingKit::cmd::interface

#endif // CLLTK_CMD_OUTPUT_HPP
