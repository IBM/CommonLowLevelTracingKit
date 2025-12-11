// Copyright (c) 2024, International Business Machines
// SPDX-License-Identifier: BSD-2-Clause-Patent

#ifndef CLLTK_CMD_DECODE_TIMESPEC_HPP
#define CLLTK_CMD_DECODE_TIMESPEC_HPP

#include <cstdint>
#include <string_view>

namespace CommonLowLevelTracingKit::cmd::interface
{

/**
 * TimeSpec - A flexible time specification parser
 *
 * Supported formats:
 *
 * Absolute:
 *   - Float seconds (Unix timestamp): "1764107189.5"
 *   - ISO 8601 datetime: "2025-11-25T21:46:29"
 *   - Extended datetime: "2025-11-25 21:46:29.5"
 *
 * Relative to now (shorthand):
 *   - -5m          - 5 minutes ago (now - 5m)
 *   - +30s         - 30 seconds from now (now + 30s)
 *
 * Anchored with optional offset:
 *   - now          - current system time
 *   - now-1m       - 1 minute ago
 *   - now+30s      - 30 seconds from now
 *   - min          - minimum timestamp in trace
 *   - min+1h       - 1 hour after trace start
 *   - max          - maximum timestamp in trace
 *   - max-5m       - 5 minutes before trace end
 *
 * Duration suffixes:
 *   - ns  - nanoseconds
 *   - us  - microseconds
 *   - ms  - milliseconds
 *   - s   - seconds (default)
 *   - m   - minutes
 *   - h   - hours
 */

struct TimeSpec {
	// Anchor points for time resolution
	enum class Anchor { Absolute, Now, Min, Max };

	Anchor anchor = Anchor::Absolute;
	int64_t offset_ns = 0;
	uint64_t absolute_ns = 0;

	static TimeSpec parse(std::string_view input); // throws on error

	[[nodiscard]] uint64_t resolve(uint64_t now_ns, uint64_t min_ns, uint64_t max_ns) const;
	[[nodiscard]] bool needs_trace_bounds() const noexcept;
	[[nodiscard]] bool is_default_min() const noexcept;
	[[nodiscard]] bool is_default_max() const noexcept;

  private:
	static int64_t parse_duration_ns(std::string_view input);
	static uint64_t parse_float_seconds_ns(std::string_view input);
	static uint64_t parse_datetime_ns(std::string_view input);
};

} // namespace CommonLowLevelTracingKit::cmd::interface

#endif // CLLTK_CMD_DECODE_TIMESPEC_HPP
