// Copyright (c) 2024, International Business Machines
// SPDX-License-Identifier: BSD-2-Clause-Patent

#include "CommonLowLevelTracingKit/snapshot.hpp"
#include "file.hpp"
#include "libtar.h"
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <sys/types.h>
#include <tar.h>
#include <vector>

using namespace CommonLowLevelTracingKit::snapshot;

// Define placeholder functions for the libtar file I/O operations
// These functions are not actually used, but are required by libtar
static int openfunc(const char * /*path*/, int /*flags*/, ...)
{
	return 0;
}
static int closefunc(int /*index*/)
{
	return 0;
}
static ssize_t readfunc(int /*index*/, void * /*data*/, size_t /*size*/)
{
	return -1;
}

// Define thread-local variables for the libtar file I/O operations
thread_local write_function_t current_writefunc;
thread_local size_t written_counter = 0;

// Define the libtar file write operation, which calls the user-provided write function
static ssize_t writefunc(int /*index*/, const void *data, size_t size)
{
	const auto rc = current_writefunc(data, size);
	if (rc.has_value() and rc.value() == size) {
		written_counter += rc.value();
		return size;
	}
	return -1;
}

// Define the libtar file I/O handler structure
// This structure specifies the functions to be called by libtar
static tartype_t handler{.openfunc = openfunc,
						 .closefunc = closefunc,
						 .readfunc = readfunc,
						 .writefunc = writefunc};

enum class ReturnCode {
	Success = 0,
	Failed,
};

ReturnCode add_file_to_tar(TAR *const tar, File *const file)
{
	th_set_type(tar, REGTYPE);
	th_set_path(tar, file->getFilepath().c_str());
	th_set_from_stat(tar, (struct stat *)&file->getFilStatus());
	th_finish(tar);
	if (th_write(tar) == -1)
		return ReturnCode::Failed;
	// loop through file and write content to archive
	for (size_t written = 0; written < file->getFilSize(); written += T_BLOCKSIZE) {
		const size_t remainding = file->getFilSize() - written;
		const size_t next_block_size = std::min<int>(remainding, T_BLOCKSIZE);
		const void *const src = file->at<char>(written);
		if (next_block_size == T_BLOCKSIZE) {
			if (tar_block_write(tar, src) == -1)
				return ReturnCode::Failed;

		} else {
			char buffer[T_BLOCKSIZE] = {0};
			memcpy(buffer, src, next_block_size);
			if (tar_block_write(tar, &buffer) == -1)
				return ReturnCode::Failed;
		}
	}
	return ReturnCode::Success;
}

// Define the main function that generates the CLLTK traces snapshot
std::optional<size_t> CommonLowLevelTracingKit::snapshot::take_snapshot(
	write_function_t func, const std::vector<std::string> &additional_tracepoints,
	const bool compress, const size_t bucket_size, const verbose_function_t &verbose)
{
	if (compress) {
		return take_snapshot_compressed(func, additional_tracepoints, bucket_size, verbose);
	}

	// get root path
	const auto tracing_path = std::getenv("CLLTK_TRACING_PATH");
	const std::filesystem::path root_path = tracing_path ? tracing_path : ".";

	// open all files
	auto files = getAllFiles(root_path);
	files.push_back(std::make_unique<VirtualFile>(additional_tracepoints));

	// Set the thread-local variables to their initial values
	current_writefunc = func;
	written_counter = 0;

	// Open a new tar archive in memory using the libtar library
	TAR *tar;
	if (0 < tar_open(&tar, (const char *)0, &handler, O_WRONLY, 0, TAR_GNU)) {
		return {}; // Return an empty optional if the archive failed to open
	}

	// add all files
	for (const auto &file : files) {
		if (verbose) {
			verbose(file->to_string(), {});
		}
		if (add_file_to_tar(tar, file.get()) != ReturnCode::Success) {
			if (verbose)
				verbose({}, "failed to add file " + file->getFilepath());
			return {};
		}
	}
	// Write an end-of-archive marker to the tar archive
	if (0 != tar_append_eof(tar)) {
		return {}; // Return an empty optional if the end-of-archive marker could not be written
	}

	// Close the tar archive
	if (0 != tar_close(tar)) {
		return {}; // Return an empty optional if the archive could not be closed
	}

	return written_counter;
}
