// Copyright (c) 2024, International Business Machines
// SPDX-License-Identifier: BSD-2-Clause-Patent

#include <cstdint>
#include <gtest/gtest.h>
#include <stdexcept>

// Include the header under test
#include "commands/timespec.hpp"

using namespace CommonLowLevelTracingKit::cmd::interface;

namespace
{

// Helper constants (int64_t to allow negative offsets)
constexpr int64_t NS_PER_SEC = 1'000'000'000LL;
constexpr int64_t NS_PER_MS = 1'000'000LL;
constexpr int64_t NS_PER_US = 1'000LL;
constexpr int64_t NS_PER_MIN = 60LL * NS_PER_SEC;
constexpr int64_t NS_PER_HOUR = 3600LL * NS_PER_SEC;

// Test fixture
class TimeSpecTest : public ::testing::Test
{
  protected:
	// Sample trace bounds for testing
	const uint64_t now_ns = 1700000000ULL * NS_PER_SEC; // ~2023-11-14
	const uint64_t min_ns = 1600000000ULL * NS_PER_SEC; // ~2020-09-13
	const uint64_t max_ns = 1650000000ULL * NS_PER_SEC; // ~2022-04-15
};

// ============================================================================
// Float seconds (Unix timestamp) tests
// ============================================================================

TEST_F(TimeSpecTest, ParseFloatSeconds_Integer)
{
	auto ts = TimeSpec::parse("1764107189");
	EXPECT_EQ(ts.anchor, TimeSpec::Anchor::Absolute);
	EXPECT_EQ(ts.absolute_ns, 1764107189ULL * NS_PER_SEC);
}

TEST_F(TimeSpecTest, ParseFloatSeconds_WithDecimal)
{
	auto ts = TimeSpec::parse("1764107189.5");
	EXPECT_EQ(ts.anchor, TimeSpec::Anchor::Absolute);
	EXPECT_EQ(ts.absolute_ns, 1764107189ULL * NS_PER_SEC + 500'000'000ULL);
}

TEST_F(TimeSpecTest, ParseFloatSeconds_SmallFraction)
{
	auto ts = TimeSpec::parse("1764107189.000001");
	EXPECT_EQ(ts.anchor, TimeSpec::Anchor::Absolute);
	EXPECT_NEAR(static_cast<double>(ts.absolute_ns),
				static_cast<double>(1764107189ULL * NS_PER_SEC + 1000), 100.0);
}

TEST_F(TimeSpecTest, ParseFloatSeconds_Zero)
{
	auto ts = TimeSpec::parse("0");
	EXPECT_EQ(ts.anchor, TimeSpec::Anchor::Absolute);
	EXPECT_EQ(ts.absolute_ns, 0ULL);
}

// ============================================================================
// DateTime format tests
// ============================================================================

TEST_F(TimeSpecTest, ParseDateTime_IsoFormat)
{
	auto ts = TimeSpec::parse("2025-11-25T21:46:29");
	EXPECT_EQ(ts.anchor, TimeSpec::Anchor::Absolute);
	EXPECT_GT(ts.absolute_ns, 0ULL);
}

TEST_F(TimeSpecTest, ParseDateTime_SpaceSeparator)
{
	auto ts = TimeSpec::parse("2025-11-25 21:46:29");
	EXPECT_EQ(ts.anchor, TimeSpec::Anchor::Absolute);
	EXPECT_GT(ts.absolute_ns, 0ULL);
}

TEST_F(TimeSpecTest, ParseDateTime_WithFractionalSeconds)
{
	auto ts = TimeSpec::parse("2025-11-25T21:46:29.123456789");
	EXPECT_EQ(ts.anchor, TimeSpec::Anchor::Absolute);
	EXPECT_GT(ts.absolute_ns, 0ULL);
}

TEST_F(TimeSpecTest, ParseDateTime_DateOnly)
{
	auto ts = TimeSpec::parse("2025-11-25");
	EXPECT_EQ(ts.anchor, TimeSpec::Anchor::Absolute);
	EXPECT_GT(ts.absolute_ns, 0ULL);
}

// ============================================================================
// "now" anchor tests
// ============================================================================

TEST_F(TimeSpecTest, ParseNow_Alone)
{
	auto ts = TimeSpec::parse("now");
	EXPECT_EQ(ts.anchor, TimeSpec::Anchor::Now);
	EXPECT_EQ(ts.offset_ns, 0);
}

TEST_F(TimeSpecTest, ParseNow_PlusSeconds)
{
	auto ts = TimeSpec::parse("now+30s");
	EXPECT_EQ(ts.anchor, TimeSpec::Anchor::Now);
	EXPECT_EQ(ts.offset_ns, 30LL * NS_PER_SEC);
}

TEST_F(TimeSpecTest, ParseNow_MinusSeconds)
{
	auto ts = TimeSpec::parse("now-30s");
	EXPECT_EQ(ts.anchor, TimeSpec::Anchor::Now);
	EXPECT_EQ(ts.offset_ns, -30LL * NS_PER_SEC);
}

TEST_F(TimeSpecTest, ParseNow_MinusMinutes)
{
	auto ts = TimeSpec::parse("now-5m");
	EXPECT_EQ(ts.anchor, TimeSpec::Anchor::Now);
	EXPECT_EQ(ts.offset_ns, -5LL * NS_PER_MIN);
}

TEST_F(TimeSpecTest, ParseNow_MinusHours)
{
	auto ts = TimeSpec::parse("now-1h");
	EXPECT_EQ(ts.anchor, TimeSpec::Anchor::Now);
	EXPECT_EQ(ts.offset_ns, -1LL * NS_PER_HOUR);
}

TEST_F(TimeSpecTest, ParseNow_PlusMilliseconds)
{
	auto ts = TimeSpec::parse("now+500ms");
	EXPECT_EQ(ts.anchor, TimeSpec::Anchor::Now);
	EXPECT_EQ(ts.offset_ns, 500LL * NS_PER_MS);
}

TEST_F(TimeSpecTest, ParseNow_PlusMicroseconds)
{
	auto ts = TimeSpec::parse("now+100us");
	EXPECT_EQ(ts.anchor, TimeSpec::Anchor::Now);
	EXPECT_EQ(ts.offset_ns, 100LL * NS_PER_US);
}

TEST_F(TimeSpecTest, ParseNow_PlusNanoseconds)
{
	auto ts = TimeSpec::parse("now+1000ns");
	EXPECT_EQ(ts.anchor, TimeSpec::Anchor::Now);
	EXPECT_EQ(ts.offset_ns, 1000LL);
}

TEST_F(TimeSpecTest, ResolveNow_Alone)
{
	auto ts = TimeSpec::parse("now");
	EXPECT_EQ(ts.resolve(now_ns, min_ns, max_ns), now_ns);
}

TEST_F(TimeSpecTest, ResolveNow_Minus1Minute)
{
	auto ts = TimeSpec::parse("now-1m");
	EXPECT_EQ(ts.resolve(now_ns, min_ns, max_ns), now_ns - NS_PER_MIN);
}

// ============================================================================
// "min" anchor tests
// ============================================================================

TEST_F(TimeSpecTest, ParseMin_Alone)
{
	auto ts = TimeSpec::parse("min");
	EXPECT_EQ(ts.anchor, TimeSpec::Anchor::Min);
	EXPECT_EQ(ts.offset_ns, 0);
}

TEST_F(TimeSpecTest, ParseMin_PlusOffset)
{
	auto ts = TimeSpec::parse("min+1h");
	EXPECT_EQ(ts.anchor, TimeSpec::Anchor::Min);
	EXPECT_EQ(ts.offset_ns, 1LL * NS_PER_HOUR);
}

TEST_F(TimeSpecTest, ParseMin_MinusOffset)
{
	auto ts = TimeSpec::parse("min-30s");
	EXPECT_EQ(ts.anchor, TimeSpec::Anchor::Min);
	EXPECT_EQ(ts.offset_ns, -30LL * NS_PER_SEC);
}

TEST_F(TimeSpecTest, ResolveMin_Alone)
{
	auto ts = TimeSpec::parse("min");
	EXPECT_EQ(ts.resolve(now_ns, min_ns, max_ns), min_ns);
}

TEST_F(TimeSpecTest, ResolveMin_Plus1Hour)
{
	auto ts = TimeSpec::parse("min+1h");
	EXPECT_EQ(ts.resolve(now_ns, min_ns, max_ns), min_ns + NS_PER_HOUR);
}

// ============================================================================
// "max" anchor tests
// ============================================================================

TEST_F(TimeSpecTest, ParseMax_Alone)
{
	auto ts = TimeSpec::parse("max");
	EXPECT_EQ(ts.anchor, TimeSpec::Anchor::Max);
	EXPECT_EQ(ts.offset_ns, 0);
}

TEST_F(TimeSpecTest, ParseMax_MinusOffset)
{
	auto ts = TimeSpec::parse("max-5m");
	EXPECT_EQ(ts.anchor, TimeSpec::Anchor::Max);
	EXPECT_EQ(ts.offset_ns, -5LL * NS_PER_MIN);
}

TEST_F(TimeSpecTest, ParseMax_PlusOffset)
{
	auto ts = TimeSpec::parse("max+10s");
	EXPECT_EQ(ts.anchor, TimeSpec::Anchor::Max);
	EXPECT_EQ(ts.offset_ns, 10LL * NS_PER_SEC);
}

TEST_F(TimeSpecTest, ResolveMax_Alone)
{
	auto ts = TimeSpec::parse("max");
	EXPECT_EQ(ts.resolve(now_ns, min_ns, max_ns), max_ns);
}

TEST_F(TimeSpecTest, ResolveMax_Minus5Minutes)
{
	auto ts = TimeSpec::parse("max-5m");
	EXPECT_EQ(ts.resolve(now_ns, min_ns, max_ns), max_ns - 5 * NS_PER_MIN);
}

// ============================================================================
// Relative shorthand tests: -30s means now - 30s, +30s means now + 30s
// ============================================================================

TEST_F(TimeSpecTest, ParseRelative_MinusSeconds)
{
	auto ts = TimeSpec::parse("-30s");
	EXPECT_EQ(ts.anchor, TimeSpec::Anchor::Now);
	EXPECT_EQ(ts.offset_ns, -30LL * NS_PER_SEC);
}

TEST_F(TimeSpecTest, ParseRelative_MinusMinutes)
{
	auto ts = TimeSpec::parse("-5m");
	EXPECT_EQ(ts.anchor, TimeSpec::Anchor::Now);
	EXPECT_EQ(ts.offset_ns, -5LL * NS_PER_MIN);
}

TEST_F(TimeSpecTest, ParseRelative_MinusHours)
{
	auto ts = TimeSpec::parse("-2h");
	EXPECT_EQ(ts.anchor, TimeSpec::Anchor::Now);
	EXPECT_EQ(ts.offset_ns, -2LL * NS_PER_HOUR);
}

TEST_F(TimeSpecTest, ParseRelative_PlusSeconds)
{
	auto ts = TimeSpec::parse("+30s");
	EXPECT_EQ(ts.anchor, TimeSpec::Anchor::Now);
	EXPECT_EQ(ts.offset_ns, 30LL * NS_PER_SEC);
}

TEST_F(TimeSpecTest, ParseRelative_PlusMinutes)
{
	auto ts = TimeSpec::parse("+5m");
	EXPECT_EQ(ts.anchor, TimeSpec::Anchor::Now);
	EXPECT_EQ(ts.offset_ns, 5LL * NS_PER_MIN);
}

TEST_F(TimeSpecTest, ParseRelative_PlusHours)
{
	auto ts = TimeSpec::parse("+2h");
	EXPECT_EQ(ts.anchor, TimeSpec::Anchor::Now);
	EXPECT_EQ(ts.offset_ns, 2LL * NS_PER_HOUR);
}

TEST_F(TimeSpecTest, ResolveRelative_Minus30Seconds)
{
	auto ts = TimeSpec::parse("-30s");
	EXPECT_EQ(ts.resolve(now_ns, min_ns, max_ns), now_ns - 30 * NS_PER_SEC);
}

TEST_F(TimeSpecTest, ResolveRelative_Minus5Minutes)
{
	auto ts = TimeSpec::parse("-5m");
	EXPECT_EQ(ts.resolve(now_ns, min_ns, max_ns), now_ns - 5 * NS_PER_MIN);
}

TEST_F(TimeSpecTest, ResolveRelative_Plus30Seconds)
{
	auto ts = TimeSpec::parse("+30s");
	EXPECT_EQ(ts.resolve(now_ns, min_ns, max_ns), now_ns + 30 * NS_PER_SEC);
}

// ============================================================================
// Duration suffix tests
// ============================================================================

TEST_F(TimeSpecTest, DurationSuffix_Nanoseconds)
{
	auto ts = TimeSpec::parse("now+1000ns");
	EXPECT_EQ(ts.offset_ns, 1000LL);
}

TEST_F(TimeSpecTest, DurationSuffix_Microseconds)
{
	auto ts = TimeSpec::parse("now+1000us");
	EXPECT_EQ(ts.offset_ns, 1000LL * NS_PER_US);
}

TEST_F(TimeSpecTest, DurationSuffix_Milliseconds)
{
	auto ts = TimeSpec::parse("now+1000ms");
	EXPECT_EQ(ts.offset_ns, 1000LL * NS_PER_MS);
}

TEST_F(TimeSpecTest, DurationSuffix_Seconds)
{
	auto ts = TimeSpec::parse("now+60s");
	EXPECT_EQ(ts.offset_ns, 60LL * NS_PER_SEC);
}

TEST_F(TimeSpecTest, DurationSuffix_Minutes)
{
	auto ts = TimeSpec::parse("now+60m");
	EXPECT_EQ(ts.offset_ns, 60LL * NS_PER_MIN);
}

TEST_F(TimeSpecTest, DurationSuffix_Hours)
{
	auto ts = TimeSpec::parse("now+24h");
	EXPECT_EQ(ts.offset_ns, 24LL * NS_PER_HOUR);
}

TEST_F(TimeSpecTest, DurationSuffix_DefaultIsSeconds)
{
	auto ts = TimeSpec::parse("now+60");
	EXPECT_EQ(ts.offset_ns, 60LL * NS_PER_SEC);
}

TEST_F(TimeSpecTest, DurationSuffix_FractionalSeconds)
{
	auto ts = TimeSpec::parse("now+1.5s");
	EXPECT_EQ(ts.offset_ns, static_cast<int64_t>(1.5 * NS_PER_SEC));
}

TEST_F(TimeSpecTest, DurationSuffix_FractionalMinutes)
{
	auto ts = TimeSpec::parse("now+0.5m");
	EXPECT_EQ(ts.offset_ns, 30LL * NS_PER_SEC);
}

// ============================================================================
// Case sensitivity tests
// ============================================================================

TEST_F(TimeSpecTest, CaseSensitive_Now)
{
	EXPECT_THROW(TimeSpec::parse("NOW"), std::invalid_argument);
	EXPECT_THROW(TimeSpec::parse("Now"), std::invalid_argument);
}

TEST_F(TimeSpecTest, CaseSensitive_Min)
{
	EXPECT_THROW(TimeSpec::parse("MIN"), std::invalid_argument);
	EXPECT_THROW(TimeSpec::parse("Min"), std::invalid_argument);
}

TEST_F(TimeSpecTest, CaseSensitive_Max)
{
	EXPECT_THROW(TimeSpec::parse("MAX"), std::invalid_argument);
	EXPECT_THROW(TimeSpec::parse("Max"), std::invalid_argument);
}

TEST_F(TimeSpecTest, CaseSensitive_LowercaseWorks)
{
	EXPECT_NO_THROW(TimeSpec::parse("now"));
	EXPECT_NO_THROW(TimeSpec::parse("min"));
	EXPECT_NO_THROW(TimeSpec::parse("max"));
}

// ============================================================================
// Whitespace handling tests
// ============================================================================

TEST_F(TimeSpecTest, Whitespace_LeadingAndTrailing)
{
	auto ts = TimeSpec::parse("  now-1m  ");
	EXPECT_EQ(ts.anchor, TimeSpec::Anchor::Now);
	EXPECT_EQ(ts.offset_ns, -1LL * NS_PER_MIN);
}

TEST_F(TimeSpecTest, Whitespace_LeadingSpace)
{
	auto ts = TimeSpec::parse(" now");
	EXPECT_EQ(ts.anchor, TimeSpec::Anchor::Now);
}

TEST_F(TimeSpecTest, Whitespace_TrailingSpace)
{
	auto ts = TimeSpec::parse("now ");
	EXPECT_EQ(ts.anchor, TimeSpec::Anchor::Now);
}

TEST_F(TimeSpecTest, Whitespace_LeadingTab)
{
	auto ts = TimeSpec::parse("\tnow");
	EXPECT_EQ(ts.anchor, TimeSpec::Anchor::Now);
}

TEST_F(TimeSpecTest, Whitespace_TrailingTab)
{
	auto ts = TimeSpec::parse("now\t");
	EXPECT_EQ(ts.anchor, TimeSpec::Anchor::Now);
}

TEST_F(TimeSpecTest, Whitespace_Mixed)
{
	auto ts = TimeSpec::parse(" \tnow\t ");
	EXPECT_EQ(ts.anchor, TimeSpec::Anchor::Now);
}

// ============================================================================
// Error handling tests
// ============================================================================

TEST_F(TimeSpecTest, Error_EmptyString)
{
	EXPECT_THROW(TimeSpec::parse(""), std::invalid_argument);
}

TEST_F(TimeSpecTest, Error_WhitespaceOnly)
{
	EXPECT_THROW(TimeSpec::parse("   "), std::invalid_argument);
}

TEST_F(TimeSpecTest, Error_InvalidAnchor)
{
	EXPECT_THROW(TimeSpec::parse("invalid"), std::invalid_argument);
}

TEST_F(TimeSpecTest, Error_InvalidSuffix)
{
	EXPECT_THROW(TimeSpec::parse("now+30x"), std::invalid_argument);
}

TEST_F(TimeSpecTest, Error_MissingOperatorAfterAnchor)
{
	EXPECT_THROW(TimeSpec::parse("now30s"), std::invalid_argument);
}

TEST_F(TimeSpecTest, Error_MissingOffsetAfterPlus)
{
	EXPECT_THROW(TimeSpec::parse("now+"), std::invalid_argument);
}

TEST_F(TimeSpecTest, Error_MissingOffsetAfterMinus)
{
	EXPECT_THROW(TimeSpec::parse("now-"), std::invalid_argument);
}

TEST_F(TimeSpecTest, Error_NegativeWithInvalidSuffix)
{
	EXPECT_THROW(TimeSpec::parse("-5x"), std::invalid_argument);
}

TEST_F(TimeSpecTest, Error_InvalidDateFormat)
{
	auto ts = TimeSpec::parse("2025/01/15");
	EXPECT_EQ(ts.anchor, TimeSpec::Anchor::Absolute);
	EXPECT_GT(ts.absolute_ns, 0ULL); // Parses "2025" as seconds
}

TEST_F(TimeSpecTest, Error_MultipleDecimalPoints)
{
	auto ts = TimeSpec::parse("1.2.3");
	EXPECT_EQ(ts.anchor, TimeSpec::Anchor::Absolute);
	EXPECT_EQ(ts.absolute_ns, 1'200'000'000ULL); // 1.2 seconds
}

TEST_F(TimeSpecTest, Error_JustSuffix)
{
	EXPECT_THROW(TimeSpec::parse("ms"), std::invalid_argument);
}

TEST_F(TimeSpecTest, Error_InvalidCharactersInNumber)
{
	auto ts = TimeSpec::parse("12abc34");
	EXPECT_EQ(ts.anchor, TimeSpec::Anchor::Absolute);
	EXPECT_EQ(ts.absolute_ns, 12ULL * NS_PER_SEC); // Parses "12"
}

// ============================================================================
// needs_trace_bounds() tests
// ============================================================================

TEST_F(TimeSpecTest, NeedsTraceBounds_Absolute)
{
	auto ts = TimeSpec::parse("1764107189.5");
	EXPECT_FALSE(ts.needs_trace_bounds());
}

TEST_F(TimeSpecTest, NeedsTraceBounds_Now)
{
	auto ts = TimeSpec::parse("now-1m");
	EXPECT_FALSE(ts.needs_trace_bounds());
}

TEST_F(TimeSpecTest, NeedsTraceBounds_Min)
{
	auto ts = TimeSpec::parse("min+1h");
	EXPECT_TRUE(ts.needs_trace_bounds());
}

TEST_F(TimeSpecTest, NeedsTraceBounds_Max)
{
	auto ts = TimeSpec::parse("max-5m");
	EXPECT_TRUE(ts.needs_trace_bounds());
}

TEST_F(TimeSpecTest, NeedsTraceBounds_RelativeToNow)
{
	auto ts = TimeSpec::parse("-30s");
	EXPECT_FALSE(ts.needs_trace_bounds()); // Now anchored to 'now', not trace bounds
}

// ============================================================================
// Default value tests
// ============================================================================

TEST_F(TimeSpecTest, IsDefaultMin)
{
	TimeSpec ts;
	ts.anchor = TimeSpec::Anchor::Absolute;
	ts.absolute_ns = 0;
	EXPECT_TRUE(ts.is_default_min());
}

TEST_F(TimeSpecTest, IsDefaultMax)
{
	TimeSpec ts;
	ts.anchor = TimeSpec::Anchor::Absolute;
	ts.absolute_ns = UINT64_MAX;
	EXPECT_TRUE(ts.is_default_max());
}

// ============================================================================
// Resolution edge cases
// ============================================================================

TEST_F(TimeSpecTest, Resolve_ClampsToZero)
{
	auto ts = TimeSpec::parse("min-1h");
	uint64_t small_min = 60ULL * NS_PER_SEC; // 1 minute
	uint64_t result = ts.resolve(0ULL, small_min, 100ULL * NS_PER_SEC);
	EXPECT_EQ(result, 0ULL);
}

TEST_F(TimeSpecTest, Resolve_ClampToZeroFromMin)
{
	auto ts = TimeSpec::parse("min-2000s");
	uint64_t small_min = 1000ULL * NS_PER_SEC; // 1000 seconds
	uint64_t result = ts.resolve(0ULL, small_min, 10000ULL * NS_PER_SEC);
	EXPECT_EQ(result, 0ULL);
}

TEST_F(TimeSpecTest, Resolve_ClampToZeroFromNow)
{
	auto ts = TimeSpec::parse("now-10000s");
	uint64_t small_now = 5000ULL * NS_PER_SEC; // 5000 seconds
	uint64_t result = ts.resolve(small_now, 0ULL, 10000ULL * NS_PER_SEC);
	EXPECT_EQ(result, 0ULL);
}

TEST_F(TimeSpecTest, Resolve_ExactZero)
{
	auto ts = TimeSpec::parse("min-1000s");
	uint64_t small_min = 1000ULL * NS_PER_SEC;
	uint64_t result = ts.resolve(0ULL, small_min, 10000ULL * NS_PER_SEC);
	EXPECT_EQ(result, 0ULL);
}

TEST_F(TimeSpecTest, Resolve_LargePositiveOffset)
{
	auto ts = TimeSpec::parse("min+50000s");
	auto result = ts.resolve(now_ns, min_ns, max_ns);
	EXPECT_EQ(result, min_ns + 50000ULL * NS_PER_SEC);
}

// ============================================================================
// Complex scenarios
// ============================================================================

TEST_F(TimeSpecTest, Scenario_Last30SecondsOfTrace)
{
	// Use max-30s for trace-relative, not -30s (which is now-relative)
	auto ts_min = TimeSpec::parse("max-30s");
	auto ts_max = TimeSpec::parse("max");

	uint64_t resolved_min = ts_min.resolve(now_ns, min_ns, max_ns);
	uint64_t resolved_max = ts_max.resolve(now_ns, min_ns, max_ns);

	EXPECT_EQ(resolved_min, max_ns - 30 * NS_PER_SEC);
	EXPECT_EQ(resolved_max, max_ns);
	EXPECT_EQ(resolved_max - resolved_min, 30 * NS_PER_SEC);
}

TEST_F(TimeSpecTest, Scenario_FirstHourOfTrace)
{
	auto ts_min = TimeSpec::parse("min");
	auto ts_max = TimeSpec::parse("min+1h");

	uint64_t resolved_min = ts_min.resolve(now_ns, min_ns, max_ns);
	uint64_t resolved_max = ts_max.resolve(now_ns, min_ns, max_ns);

	EXPECT_EQ(resolved_min, min_ns);
	EXPECT_EQ(resolved_max, min_ns + NS_PER_HOUR);
}

TEST_F(TimeSpecTest, Scenario_Last5MinutesFromNow)
{
	auto ts_min = TimeSpec::parse("now-5m");
	auto ts_max = TimeSpec::parse("now");

	uint64_t resolved_min = ts_min.resolve(now_ns, min_ns, max_ns);
	uint64_t resolved_max = ts_max.resolve(now_ns, min_ns, max_ns);

	EXPECT_EQ(resolved_min, now_ns - 5 * NS_PER_MIN);
	EXPECT_EQ(resolved_max, now_ns);
}

} // namespace
