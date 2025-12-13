// Copyright (c) 2024, International Business Machines
// SPDX-License-Identifier: BSD-2-Clause-Patent

#pragma once

#include "CommonLowLevelTracingKit/decoder/Inline.hpp"
#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>

namespace CommonLowLevelTracingKit::decoder::source::low_level {

	/**
	 * @brief Utility class for timestamp formatting
	 *
	 * Provides efficient timestamp-to-string conversion with thread-local caching
	 * for same-second timestamps to avoid redundant date calculations.
	 */
	class ToString final {
		ToString() = delete;

		// write v as decimal into buf backwards; returns pointer to first digit
		static INLINE char *write_digits_rev(char *buf_end, uint64_t v) {
			char *p = buf_end;
			do {
				*--p = char('0' + (v % 10));
				v /= 10;
			} while (v);
			return p;
		}

		template <size_t width> static INLINE void write_digits(char *p, uint32_t v) {
			for (int i = width - 1; i >= 0; --i) {
				p[i] = static_cast<char>('0' + (v % 10));
				v /= 10;
			}
		}

		template <size_t width> static INLINE void write_digits(char *p, int32_t v) {
			for (int i = width - 1; i >= 0; --i) {
				p[i] = static_cast<char>('0' + (v % 10));
				v /= 10;
			}
		}

		// Fast UTC date calculation without timezone lookup
		// Based on Howard Hinnant's date algorithms:
		// http://howardhinnant.github.io/date_algorithms.html
		struct DateTimeUTC {
			int32_t year;
			uint32_t month;
			uint32_t day;
			uint32_t hour;
			uint32_t minute;
			uint32_t second;
		};

		static DateTimeUTC unix_to_utc(int64_t unix_sec) noexcept;

	  public:
		// Buffer size requirements for formatted strings
		static constexpr size_t DATE_AND_TIME_BUF_SIZE = 30; // "YYYY-MM-DD HH:MM:SS.nnnnnnnnn\0"
		static constexpr size_t TIMESTAMP_NS_BUF_SIZE = 32;	 // " SSSSSSSSSSS.nnnnnnnnn\0"

		/**
		 * @brief Write date/time string to buffer (no allocation)
		 * @param buf Buffer of at least DATE_AND_TIME_BUF_SIZE bytes
		 * @param ts Timestamp in nanoseconds
		 * @return Pointer to start of formatted string in buffer
		 */
		static const char *date_and_time_to(char *buf, uint64_t ts);

		/**
		 * @brief Write timestamp string to buffer (no allocation)
		 * @param buf Buffer of at least TIMESTAMP_NS_BUF_SIZE bytes
		 * @param ts Timestamp in nanoseconds
		 * @return Pointer to start of formatted string in buffer
		 */
		static const char *timestamp_ns_to(char *buf, uint64_t ts);

		/**
		 * @brief Convert timestamp to date/time string
		 * @param ts Timestamp in nanoseconds
		 * @return Formatted string "YYYY-MM-DD HH:MM:SS.nnnnnnnnn"
		 */
		static std::string date_and_time(uint64_t ts);

		/**
		 * @brief Convert timestamp to nanosecond string
		 * @param ts Timestamp in nanoseconds
		 * @return Formatted string with seconds and nanoseconds
		 */
		static std::string timestamp_ns(uint64_t ts);
	};

}; // namespace CommonLowLevelTracingKit::decoder::source::low_level
