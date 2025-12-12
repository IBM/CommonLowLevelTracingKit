// Copyright (c) 2024, International Business Machines
// SPDX-License-Identifier: BSD-2-Clause-Patent

#include "CommonLowLevelTracingKit/decoder/ToString.hpp"
#include <algorithm>
#include <array>

namespace CommonLowLevelTracingKit::decoder::source::low_level {

	// ============================================================================
	// ToString Implementation
	// ============================================================================

	ToString::DateTimeUTC ToString::unix_to_utc(int64_t unix_sec) noexcept {
		// Extract time of day
		int64_t days = unix_sec / 86400;
		int32_t secs = static_cast<int32_t>(unix_sec % 86400);
		if (secs < 0) {
			secs += 86400;
			days -= 1;
		}

		DateTimeUTC dt;
		dt.second = static_cast<uint32_t>(secs % 60);
		secs /= 60;
		dt.minute = static_cast<uint32_t>(secs % 60);
		dt.hour = static_cast<uint32_t>(secs / 60);

		// Civil date from days since 1970-01-01 (Howard Hinnant's algorithm)
		// Shift epoch from 1970-01-01 to 0000-03-01 (eliminates leap year special case)
		days += 719468;

		const int64_t era = (days >= 0 ? days : days - 146096) / 146097;
		const uint32_t doe = static_cast<uint32_t>(days - era * 146097); // day of era [0, 146096]
		const uint32_t yoe =
			(doe - doe / 1460 + doe / 36524 - doe / 146096) / 365; // year of era [0, 399]
		const int64_t y = static_cast<int64_t>(yoe) + era * 400;
		const uint32_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100); // day of year [0, 365]
		const uint32_t mp = (5 * doy + 2) / 153;					  // month [0, 11]
		dt.day = doy - (153 * mp + 2) / 5 + 1;						  // day [1, 31]
		dt.month = mp < 10 ? mp + 3 : mp - 9;						  // month [1, 12]
		dt.year = static_cast<int32_t>(y + (dt.month <= 2 ? 1 : 0));

		return dt;
	}

	const char *ToString::date_and_time_to(char *buf, uint64_t ts) {
		const int64_t sec = static_cast<int64_t>(ts / 1'000'000'000ULL);
		const uint32_t nsec = static_cast<uint32_t>(ts % 1'000'000'000ULL);

		// Cache the date/time prefix for same-second timestamps
		thread_local int64_t cached_sec = -1;
		thread_local std::array<char, DATE_AND_TIME_BUF_SIZE> cached_buf{
			"YYYY-MM-DD HH:MM:SS.nnnnnnnnn"};

		if (sec != cached_sec) {
			cached_sec = sec;
			const DateTimeUTC dt = unix_to_utc(sec);

			write_digits<4>(&cached_buf[0], static_cast<uint32_t>(dt.year)); // YYYY
			write_digits<2>(&cached_buf[5], dt.month);						 // MM
			write_digits<2>(&cached_buf[8], dt.day);						 // DD
			write_digits<2>(&cached_buf[11], dt.hour);						 // HH
			write_digits<2>(&cached_buf[14], dt.minute);					 // MM
			write_digits<2>(&cached_buf[17], dt.second);					 // SS
		}

		// Always update nanoseconds (they change within the same second)
		write_digits<9>(&cached_buf[20], nsec); // nnnnnnnnn

		// Copy to output buffer
		std::memcpy(buf, cached_buf.data(), DATE_AND_TIME_BUF_SIZE - 1);
		buf[DATE_AND_TIME_BUF_SIZE - 1] = '\0';
		return buf;
	}

	const char *ToString::timestamp_ns_to(char *buf, uint64_t ts) {
		constexpr size_t decimal_digits = 9;
		constexpr size_t min_size = 20;

		const uint64_t sec = ts / 1'000'000'000ULL;
		const uint32_t nsec = static_cast<uint32_t>(ts % 1'000'000'000ULL);

		// Work backwards from end of buffer
		char *const buf_end = buf + TIMESTAMP_NS_BUF_SIZE - 1;
		*buf_end = '\0';

		char *const dot_position = buf_end - decimal_digits - 1;
		*dot_position = '.';
		write_digits<decimal_digits>(dot_position + 1, nsec);

		char *const number_start = write_digits_rev(dot_position, sec);
		char *const min_start = buf_end - min_size;
		char *const start = std::min(min_start, number_start);
		for (char *c = start; c < number_start; c++) { *c = ' '; }

		return start;
	}

	std::string ToString::date_and_time(uint64_t ts) {
		const int64_t sec = static_cast<int64_t>(ts / 1'000'000'000ULL);
		const uint32_t nsec = static_cast<uint32_t>(ts % 1'000'000'000ULL);

		// Cache the date/time prefix for same-second timestamps
		// This avoids recalculating date for tracepoints within the same second
		thread_local int64_t cached_sec = -1;
		thread_local std::array<char, 30> cached_buf{"YYYY-MM-DD HH:MM:SS.nnnnnnnnn"};

		if (sec != cached_sec) {
			cached_sec = sec;
			const DateTimeUTC dt = unix_to_utc(sec);

			write_digits<4>(&cached_buf[0], static_cast<uint32_t>(dt.year)); // YYYY
			write_digits<2>(&cached_buf[5], dt.month);						 // MM
			write_digits<2>(&cached_buf[8], dt.day);						 // DD
			write_digits<2>(&cached_buf[11], dt.hour);						 // HH
			write_digits<2>(&cached_buf[14], dt.minute);					 // MM
			write_digits<2>(&cached_buf[17], dt.second);					 // SS
		}

		// Always update nanoseconds (they change within the same second)
		write_digits<9>(&cached_buf[20], nsec); // nnnnnnnnn

		return std::string(cached_buf.data(), cached_buf.size() - 1);
	}

	std::string ToString::timestamp_ns(uint64_t ts) {
		constexpr size_t decimal_digits = 9;
		std::array<char, 30> buf{};

		const uint64_t sec = ts / 1'000'000'000ULL;
		const uint32_t nsec = static_cast<uint32_t>(ts % 1'000'000'000ULL);

		buf.back() = '\0';
		char *const dot_position = (buf.data() + buf.size() - decimal_digits - 2);
		*dot_position = '.';
		write_digits<decimal_digits>(dot_position + 1, nsec); // nnnnnnnnn

		char *const number_start = write_digits_rev(dot_position, sec);
		constexpr size_t min_size = 20;
		char *const min_start = (buf.data() + buf.size() - min_size - 1);
		char *const start = std::min(min_start, number_start);
		for (char *c = start; c < number_start; c++) { *c = ' '; }
		return std::string(start);
	}

} // namespace CommonLowLevelTracingKit::decoder::source::low_level
