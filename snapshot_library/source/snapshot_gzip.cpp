// Copyright (c) 2024, International Business Machines
// SPDX-License-Identifier: BSD-2-Clause-Patent

#include "CommonLowLevelTracingKit/snapshot.hpp"
#include <cstdint>
#include <functional>
#include <optional>
#include <stddef.h>
#include <string>
#include <vector>
#include <zconf.h>
#include <zlib.h>

using namespace CommonLowLevelTracingKit::snapshot;

std::optional<size_t> CommonLowLevelTracingKit::snapshot::take_snapshot_compressed(
	write_function_t output_write, const std::vector<std::string> &additional_tracepoints,
	const size_t bucket_size, const verbose_function_t &verbose)
{
	z_stream strm;
	strm.zalloc = Z_NULL;
	strm.zfree = Z_NULL;
	strm.opaque = Z_NULL;
	deflateInit2(&strm, Z_BEST_SPEED, Z_DEFLATED, 15 + 16, 8, Z_DEFAULT_STRATEGY);

	std::vector<uint8_t> compressorStage(bucket_size);
	strm.avail_out = compressorStage.size();
	strm.next_out = compressorStage.data();

	size_t totalOutputed = 0;
	size_t totalGotFromTar = 0;
	const auto tar_write_function = [&](const void *gotFromTar,
										size_t gotFromTarSize) -> std::optional<size_t> {
		totalGotFromTar += gotFromTarSize;

		strm.avail_in = gotFromTarSize;
		strm.next_in = static_cast<Bytef *>(const_cast<void *>(gotFromTar));
		do {
			deflate(&strm, Z_NO_FLUSH);
			if (strm.avail_out == 0) {
				const auto rc = output_write(compressorStage.data(), compressorStage.size());
				if (not rc.has_value() or rc.value() != compressorStage.size())
					return std::nullopt;
				totalOutputed += compressorStage.size();

				strm.avail_out = compressorStage.size();
				strm.next_out = compressorStage.data();
			}
		} while (strm.avail_in > 0);
		return gotFromTarSize;
	};
	auto tarReturnValue =
		take_snapshot(tar_write_function, additional_tracepoints, false, bucket_size, verbose);
	if (not tarReturnValue.has_value() or tarReturnValue.value() != totalGotFromTar) {
		deflateEnd(&strm);
		return std::nullopt;
	}
	int deflate_result;
	do {
		deflate_result = deflate(&strm, Z_FINISH);
		const size_t remaingingInCompressorStage = compressorStage.size() - strm.avail_out;
		const auto rc = output_write(compressorStage.data(), remaingingInCompressorStage);
		if (not rc.has_value() or rc.value() != remaingingInCompressorStage) {
			deflateEnd(&strm);
			return std::nullopt;
		}
		totalOutputed += rc.value();
		strm.avail_out = compressorStage.size();
		strm.next_out = compressorStage.data();
	} while (deflate_result != Z_STREAM_END);
	deflateEnd(&strm);
	return totalOutputed;
}
