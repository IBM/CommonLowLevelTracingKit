// Copyright (c) 2024, International Business Machines
// SPDX-License-Identifier: BSD-2-Clause-Patent

#ifndef CLLTK_CMD_DECODE_TIMESPEC_HPP
#define CLLTK_CMD_DECODE_TIMESPEC_HPP

#include <cstdint>
#include <string_view>

namespace CommonLowLevelTracingKit::cmd::decode
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
 * Anchored with optional offset:
 *   - now          - current system time
 *   - now-1m       - 1 minute ago
 *   - now+30s      - 30 seconds from now
 *   - min          - minimum timestamp in trace
 *   - min+1h       - 1 hour after trace start
 *   - max          - maximum timestamp in trace
 *   - max-5m       - 5 minutes before trace end
 *
 * Relative (Python-style, from trace end):
 *   - -30s         - 30 seconds before trace end
 *   - -5m          - 5 minutes before trace end
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
	enum class Anchor { Absolute, Now, Min, Max, RelativeToMax };

	Anchor anchor = Anchor::Absolute;
	int64_t offset_ns = 0;	  // Offset from anchor (can be negative)
	uint64_t absolute_ns = 0; // Used when anchor == Absolute

	// Parse a time specification string
	// Throws std::invalid_argument on parse error
	static TimeSpec parse(std::string_view input);

	// Resolve to absolute nanoseconds given trace bounds
	// now_ns: current time in nanoseconds (Unix epoch)
	// min_ns: minimum timestamp in trace
	// max_ns: maximum timestamp in trace
	[[nodiscard]] uint64_t resolve(uint64_t now_ns, uint64_t min_ns, uint64_t max_ns) const;

	// Check if this TimeSpec requires trace bounds to resolve
	[[nodiscard]] bool needs_trace_bounds() const noexcept;

	// Check if this is the default (unset) value
	[[nodiscard]] bool is_default_min() const noexcept;
	[[nodiscard]] bool is_default_max() const noexcept;

  private:
	// Parse duration with suffix (e.g., "30s", "5m", "1h")
	static int64_t parse_duration_ns(std::string_view input);

	// Parse float seconds to nanoseconds
	static uint64_t parse_float_seconds_ns(std::string_view input);

	// Parse ISO datetime to nanoseconds
	static uint64_t parse_datetime_ns(std::string_view input);
};

} // namespace CommonLowLevelTracingKit::cmd::decode

#endif // CLLTK_CMD_DECODE_TIMESPEC_HPP
