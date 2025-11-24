// Copyright (c) 2024, International Business Machines
// SPDX-License-Identifier: BSD-2-Clause-Patent

#include "CommonLowLevelTracingKit/snapshot/snapshot.hpp"
#include "file.hpp"
#include <algorithm>
#include <archive.h>
#include <archive_entry.h>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <sys/types.h>
#include <vector>

using namespace CommonLowLevelTracingKit::snapshot;

// Context structure to pass user callback and counter through libarchive
struct archive_write_context {
	write_function_t write_func;
	size_t written_counter;
};

// libarchive write callback that calls the user-provided write function
static la_ssize_t clltk_archive_write_callback(struct archive * /*a*/, void *client_data,
											   const void *buffer, size_t length)
{
	auto *ctx = static_cast<archive_write_context *>(client_data);
	const auto rc = ctx->write_func(buffer, length);
	if (rc.has_value() and rc.value() == length) {
		ctx->written_counter += rc.value();
		return static_cast<la_ssize_t>(length);
	}
	return -1;
}

enum class ReturnCode {
	Success = 0,
	Failed,
};

ReturnCode add_file_to_archive(struct archive *a, File *const file)
{
	struct archive_entry *entry = archive_entry_new();
	if (entry == nullptr)
		return ReturnCode::Failed;

	// Set file metadata
	archive_entry_copy_stat(entry, &file->getFilStatus());
	archive_entry_set_pathname(entry, file->getFilepath().c_str());

	// Write header
	if (archive_write_header(a, entry) != ARCHIVE_OK) {
		archive_entry_free(entry);
		return ReturnCode::Failed;
	}

	// Write file data
	const ssize_t written = archive_write_data(a, file->at<char>(0), file->getFilSize());
	archive_entry_free(entry);

	if (written < 0 or static_cast<size_t>(written) != file->getFilSize())
		return ReturnCode::Failed;

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

	// Create context for write callback
	archive_write_context ctx{.write_func = func, .written_counter = 0};

	// add custom deleter to automatically free archive struct in any case
	using archive_ptr = std::unique_ptr<struct archive, decltype(&archive_write_free)>;
	archive_ptr a = archive_ptr(nullptr, archive_write_free);
	{
		// Open a new tar archive using libarchive
		struct archive *raw_archive = archive_write_new();
		if (raw_archive == nullptr) {
			return {}; // Return an empty optional if the archive failed to create
		}

		// Set format to GNU tar for compatibility
		if (archive_write_set_format_gnutar(raw_archive) != ARCHIVE_OK) {
			archive_write_free(raw_archive);
			return {};
		}

		// Open archive with custom write callback
		if (archive_write_open(raw_archive, &ctx, nullptr, clltk_archive_write_callback, nullptr) !=
			ARCHIVE_OK) {
			archive_write_free(raw_archive);
			return {};
		}

		a = archive_ptr(raw_archive, archive_write_free);
	}

	// add all files
	for (const auto &file : files) {
		if (verbose) {
			verbose(file->to_string(), {});
		}
		if (add_file_to_archive(a.get(), file.get()) != ReturnCode::Success) {
			if (verbose)
				verbose({}, "failed to add file " + file->getFilepath());
			return {};
		}
	}

	// Close the archive (writes end-of-archive marker)
	if (archive_write_close(a.get()) != ARCHIVE_OK) {
		return {}; // Return an empty optional if the archive could not be closed
	}

	// archive will be automatically freed by custom deleter in unique_ptr
	return ctx.written_counter;
}
