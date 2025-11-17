#pragma once

#include "inline.hpp"
#include <array>
#include <cstdint>
#include <cstdlib>
#include <ctime>
#include <string>

namespace CommonLowLevelTracingKit::decoder::source::low_level {
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
				p[i] = (char)('0' + (v % 10));
				v /= 10;
			}
		}
		template <size_t width> static INLINE void write_digits(char *p, int32_t v) {
			for (int i = width - 1; i >= 0; --i) {
				p[i] = (char)('0' + (v % 10));
				v /= 10;
			}
		}

	  public:
		static CONST_INLINE std::string date_and_time(uint64_t ts) {
			time_t sec = (time_t)(ts / 1'000'000'000ULL);
			uint32_t nsec = uint32_t(ts % 1'000'000'000ULL);

			std::tm tm;
			gmtime_r(&sec, &tm);

			std::array<char, 30> buf{"YYYY-MM-DD HH:MM:SS.nnnnnnnnn"};
			static_assert(sizeof("YYYY-MM-DD HH:MM:SS.nnnnnnnnn") == sizeof(buf));

			write_digits<4>(&buf[0], tm.tm_year + 1900); // YYYY
			write_digits<2>(&buf[5], tm.tm_mon + 1);	 // MM
			write_digits<2>(&buf[8], tm.tm_mday);		 // DD
			write_digits<2>(&buf[11], tm.tm_hour);		 // HH
			write_digits<2>(&buf[14], tm.tm_min);		 // MM
			write_digits<2>(&buf[17], tm.tm_sec);		 // SS
			write_digits<9>(&buf[20], nsec);			 // nnnnnnnnn

			// make std::string once
			return std::string(buf.data(), buf.size() - 1);
		}
		static CONST_INLINE std::string timestamp_ns(uint64_t ts) {
			constexpr size_t decimal_digits = 9;
			std::array<char, 30> buf{};

			const uint64_t sec = ts / 1'000'000'000ULL;
			const uint32_t nsec = uint32_t(ts % 1'000'000'000ULL);

			buf.back() = '\0';
			char *const dot_position = (buf.data() + buf.size() - decimal_digits - 2);
			*dot_position = '.';
			write_digits<decimal_digits>(dot_position + 1, nsec); // nnnnnnnnn

			char *const number_start = write_digits_rev(dot_position, sec);
			constexpr size_t min_size = 20;
			char *const min_start = (buf.data() + buf.size() - min_size - 1);
			char *const start = std::min(min_start, number_start);
			for (char *c = start; c < number_start; c++) *c = ' ';
			return std::string(start);
		}
	};
}; // namespace CommonLowLevelTracingKit::decoder::source::low_level
