// Copyright (c) 2024, International Business Machines
// SPDX-License-Identifier: BSD-2-Clause-Patent

#include "commands/timespec.hpp"

#include <ctime>
#include <stdexcept>
#include <string>

namespace CommonLowLevelTracingKit::cmd::interface
{

bool TimeSpec::needs_trace_bounds() const noexcept
{
	return anchor == Anchor::Min || anchor == Anchor::Max;
}

bool TimeSpec::is_default_min() const noexcept
{
	return anchor == Anchor::Absolute && absolute_ns == 0;
}

bool TimeSpec::is_default_max() const noexcept
{
	return anchor == Anchor::Absolute && absolute_ns == UINT64_MAX;
}

int64_t TimeSpec::parse_duration_ns(std::string_view input)
{
	if (input.empty())
		throw std::invalid_argument("Empty duration string");

	// Find where the number ends and suffix begins
	size_t suffix_start = 0;
	bool has_dot = false;
	for (size_t i = 0; i < input.size(); ++i) {
		char c = input[i];
		if (c == '.') {
			if (has_dot)
				break;
			has_dot = true;
		} else if (c < '0' || c > '9') {
			suffix_start = i;
			break;
		} else {
			suffix_start = i + 1;
		}
	}

	std::string_view number_part = input.substr(0, suffix_start);
	std::string_view suffix = input.substr(suffix_start);

	if (number_part.empty())
		throw std::invalid_argument("No numeric value in duration: " + std::string(input));

	// Parse the number as double to handle fractional values
	double value = std::stod(std::string(number_part));

	// Determine multiplier based on suffix
	int64_t multiplier_ns = 1'000'000'000LL; // Default: seconds
	if (!suffix.empty()) {
		if (suffix == "ns")
			multiplier_ns = 1LL;
		else if (suffix == "us")
			multiplier_ns = 1'000LL;
		else if (suffix == "ms")
			multiplier_ns = 1'000'000LL;
		else if (suffix == "s")
			multiplier_ns = 1'000'000'000LL;
		else if (suffix == "m")
			multiplier_ns = 60LL * 1'000'000'000LL;
		else if (suffix == "h")
			multiplier_ns = 3600LL * 1'000'000'000LL;
		else
			throw std::invalid_argument("Unknown duration suffix: " + std::string(suffix));
	}

	return static_cast<int64_t>(value * static_cast<double>(multiplier_ns));
}

uint64_t TimeSpec::parse_float_seconds_ns(std::string_view input)
{
	try {
		double seconds = std::stod(std::string(input));
		if (seconds < 0)
			throw std::invalid_argument("Negative timestamp not allowed: " + std::string(input));
		return static_cast<uint64_t>(seconds * 1'000'000'000.0);
	} catch (const std::invalid_argument &) {
		throw std::invalid_argument("Invalid timestamp format: " + std::string(input));
	} catch (const std::out_of_range &) {
		throw std::invalid_argument("Timestamp out of range: " + std::string(input));
	}
}

uint64_t TimeSpec::parse_datetime_ns(std::string_view input)
{
	// Supported formats:
	// "2025-11-25T21:46:29"
	// "2025-11-25 21:46:29"
	// "2025-11-25T21:46:29.123456789"
	// "2025-11-25 21:46:29.123456789"

	std::string str(input);

	// Replace 'T' with space for uniform parsing
	for (char &c : str) {
		if (c == 'T')
			c = ' ';
	}

	std::tm tm = {};
	double fractional_seconds = 0.0;

	// Parse date and time parts
	int year, month, day, hour = 0, minute = 0, second = 0;
	char sep1, sep2, sep3, sep4;

	// Try full datetime with fractional seconds
	size_t dot_pos = str.find('.');
	if (dot_pos != std::string::npos) {
		std::string datetime_part = str.substr(0, dot_pos);
		std::string frac_part = str.substr(dot_pos);

		int parsed = sscanf(datetime_part.c_str(), "%d%c%d%c%d %d%c%d%c%d", &year, &sep1, &month,
							&sep2, &day, &hour, &sep3, &minute, &sep4, &second);
		if (parsed < 3)
			throw std::invalid_argument("Invalid datetime format: " + std::string(input));

		fractional_seconds = std::stod(frac_part);
	} else {
		int parsed = sscanf(str.c_str(), "%d%c%d%c%d %d%c%d%c%d", &year, &sep1, &month, &sep2, &day,
							&hour, &sep3, &minute, &sep4, &second);
		if (parsed < 3)
			throw std::invalid_argument("Invalid datetime format: " + std::string(input));
	}

	tm.tm_year = year - 1900;
	tm.tm_mon = month - 1;
	tm.tm_mday = day;
	tm.tm_hour = hour;
	tm.tm_min = minute;
	tm.tm_sec = second;
	tm.tm_isdst = -1; // Let system determine DST

	// Convert to time_t (seconds since epoch)
	time_t epoch_seconds = timegm(&tm);
	if (epoch_seconds == -1)
		throw std::invalid_argument("Invalid datetime: " + std::string(input));

	// Convert to nanoseconds
	uint64_t ns = static_cast<uint64_t>(epoch_seconds) * 1'000'000'000ULL;
	ns += static_cast<uint64_t>(fractional_seconds * 1'000'000'000.0);

	return ns;
}

TimeSpec TimeSpec::parse(std::string_view input)
{
	TimeSpec result;

	// Trim whitespace
	while (!input.empty() && (input.front() == ' ' || input.front() == '\t'))
		input.remove_prefix(1);
	while (!input.empty() && (input.back() == ' ' || input.back() == '\t'))
		input.remove_suffix(1);

	if (input.empty())
		throw std::invalid_argument("Empty time specification");

	// Check for anchored formats: now, min, max
	if (input.starts_with("now")) {
		result.anchor = Anchor::Now;
		input.remove_prefix(3);

		if (!input.empty()) {
			// Parse offset: +duration or -duration
			if (input[0] == '+') {
				input.remove_prefix(1);
				result.offset_ns = parse_duration_ns(input);
			} else if (input[0] == '-') {
				input.remove_prefix(1);
				result.offset_ns = -parse_duration_ns(input);
			} else {
				throw std::invalid_argument("Expected +/- after 'now': " + std::string(input));
			}
		}
		return result;
	}

	if (input.starts_with("min")) {
		result.anchor = Anchor::Min;
		input.remove_prefix(3);

		if (!input.empty()) {
			if (input[0] == '+') {
				input.remove_prefix(1);
				result.offset_ns = parse_duration_ns(input);
			} else if (input[0] == '-') {
				input.remove_prefix(1);
				result.offset_ns = -parse_duration_ns(input);
			} else {
				throw std::invalid_argument("Expected +/- after 'min': " + std::string(input));
			}
		}
		return result;
	}

	if (input.starts_with("max")) {
		result.anchor = Anchor::Max;
		input.remove_prefix(3);

		if (!input.empty()) {
			if (input[0] == '+') {
				input.remove_prefix(1);
				result.offset_ns = parse_duration_ns(input);
			} else if (input[0] == '-') {
				input.remove_prefix(1);
				result.offset_ns = -parse_duration_ns(input);
			} else {
				throw std::invalid_argument("Expected +/- after 'max': " + std::string(input));
			}
		}
		return result;
	}

	// Check for relative format: -5m (now minus duration), +5m (now plus duration)
	if (input[0] == '-') {
		// If next char is a digit, it's relative duration from now
		if (input.size() > 1 && input[1] >= '0' && input[1] <= '9') {
			result.anchor = Anchor::Now;
			input.remove_prefix(1);
			result.offset_ns = -parse_duration_ns(input);
			return result;
		}
	}

	if (input[0] == '+') {
		// If next char is a digit, it's relative duration from now
		if (input.size() > 1 && input[1] >= '0' && input[1] <= '9') {
			result.anchor = Anchor::Now;
			input.remove_prefix(1);
			result.offset_ns = parse_duration_ns(input);
			return result;
		}
	}

	// Check for datetime format (contains '-' as date separator)
	// Look for pattern like "2025-11-25" or "2025-11-25T" or "2025-11-25 "
	bool looks_like_datetime = false;
	if (input.size() >= 10) {
		// Check for YYYY-MM-DD pattern
		if (input[4] == '-' && input[7] == '-') {
			looks_like_datetime = true;
		}
	}

	if (looks_like_datetime) {
		result.anchor = Anchor::Absolute;
		result.absolute_ns = parse_datetime_ns(input);
		return result;
	}

	// Otherwise, try to parse as float seconds (Unix timestamp)
	result.anchor = Anchor::Absolute;
	result.absolute_ns = parse_float_seconds_ns(input);
	return result;
}

uint64_t TimeSpec::resolve(uint64_t now_ns, uint64_t min_ns, uint64_t max_ns) const
{
	int64_t base_ns = 0;

	switch (anchor) {
	case Anchor::Absolute:
		return absolute_ns;

	case Anchor::Now:
		base_ns = static_cast<int64_t>(now_ns);
		break;

	case Anchor::Min:
		base_ns = static_cast<int64_t>(min_ns);
		break;

	case Anchor::Max:
		base_ns = static_cast<int64_t>(max_ns);
		break;
	}

	int64_t result = base_ns + offset_ns;

	// Clamp to valid range
	if (result < 0)
		return 0;
	return static_cast<uint64_t>(result);
}

} // namespace CommonLowLevelTracingKit::cmd::interface
