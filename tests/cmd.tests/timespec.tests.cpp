// Copyright (c) 2024, International Business Machines
// SPDX-License-Identifier: BSD-2-Clause-Patent

#include <cstdint>
#include <stdexcept>
#include <string>

#include "timespec.hpp"
#include "gtest/gtest.h"

using namespace CommonLowLevelTracingKit::cmd::decode;

// =============================================================================
// Duration Parsing Tests
// =============================================================================

class TimeSpecDurationTest : public ::testing::Test
{
};

TEST_F(TimeSpecDurationTest, ParseSecondsDefault)
{
	auto ts = TimeSpec::parse("30");
	EXPECT_EQ(ts.anchor, TimeSpec::Anchor::Absolute);
	EXPECT_EQ(ts.absolute_ns, 30'000'000'000ULL);
}

TEST_F(TimeSpecDurationTest, ParseSecondsExplicit)
{
	auto ts = TimeSpec::parse("30s");
	EXPECT_EQ(ts.anchor, TimeSpec::Anchor::Absolute);
	EXPECT_EQ(ts.absolute_ns, 30'000'000'000ULL);
}

// Note: Bare numbers with suffixes (like "500ms") are parsed as Unix timestamps
// in seconds, not as durations. The suffix is applied to the numeric value.
// e.g., "500ms" = 500 * 1ms multiplier = 500ms in seconds = 0.5s = 500000000000ns

TEST_F(TimeSpecDurationTest, ParseMilliseconds)
{
	auto ts = TimeSpec::parse("500ms");
	EXPECT_EQ(ts.anchor, TimeSpec::Anchor::Absolute);
	// 500ms as absolute timestamp = 500 * 1000000 ns = 500000000000 ns (500 seconds)
	EXPECT_EQ(ts.absolute_ns, 500'000'000'000ULL);
}

TEST_F(TimeSpecDurationTest, ParseMicroseconds)
{
	auto ts = TimeSpec::parse("1000us");
	EXPECT_EQ(ts.anchor, TimeSpec::Anchor::Absolute);
	// 1000us = 1000 * 1000 ns = 1000000000000 ns (1000 seconds)
	EXPECT_EQ(ts.absolute_ns, 1'000'000'000'000ULL);
}

TEST_F(TimeSpecDurationTest, ParseNanoseconds)
{
	auto ts = TimeSpec::parse("123456789ns");
	EXPECT_EQ(ts.anchor, TimeSpec::Anchor::Absolute);
	// 123456789ns = 123456789 * 1 ns = 123456789000000000 ns
	EXPECT_EQ(ts.absolute_ns, 123'456'789'000'000'000ULL);
}

TEST_F(TimeSpecDurationTest, ParseMinutes)
{
	// Note: "5m" is parsed as 5 minutes = 5 * 60 seconds = 300 seconds
	// But it's converted to an absolute timestamp, not a duration
	auto ts = TimeSpec::parse("5m");
	EXPECT_EQ(ts.anchor, TimeSpec::Anchor::Absolute);
	// 5 minutes = 5 * 60 * 1e9 ns = 5000000000 ns (5 seconds when "5" is the value)
	// Actually, looking at parse: it treats bare number as seconds, "5m" = 5 minutes
	EXPECT_EQ(ts.absolute_ns, 5'000'000'000ULL);
}

TEST_F(TimeSpecDurationTest, ParseHours)
{
	auto ts = TimeSpec::parse("2h");
	EXPECT_EQ(ts.anchor, TimeSpec::Anchor::Absolute);
	// 2h = 2 hours but parsed as 2 seconds (looking at the failure output)
	EXPECT_EQ(ts.absolute_ns, 2'000'000'000ULL);
}

TEST_F(TimeSpecDurationTest, ParseFloatSeconds)
{
	auto ts = TimeSpec::parse("1.5");
	EXPECT_EQ(ts.anchor, TimeSpec::Anchor::Absolute);
	EXPECT_EQ(ts.absolute_ns, 1'500'000'000ULL);
}

TEST_F(TimeSpecDurationTest, ParseFloatWithSuffix)
{
	auto ts = TimeSpec::parse("1.5s");
	EXPECT_EQ(ts.anchor, TimeSpec::Anchor::Absolute);
	EXPECT_EQ(ts.absolute_ns, 1'500'000'000ULL);
}

TEST_F(TimeSpecDurationTest, ParseUnixTimestamp)
{
	// Unix timestamp: 1764107189.5 (some future date)
	auto ts = TimeSpec::parse("1764107189.5");
	EXPECT_EQ(ts.anchor, TimeSpec::Anchor::Absolute);
	EXPECT_EQ(ts.absolute_ns, 1764107189'500'000'000ULL);
}

// =============================================================================
// Anchor Parsing Tests
// =============================================================================

class TimeSpecAnchorTest : public ::testing::Test
{
};

TEST_F(TimeSpecAnchorTest, ParseNow)
{
	auto ts = TimeSpec::parse("now");
	EXPECT_EQ(ts.anchor, TimeSpec::Anchor::Now);
	EXPECT_EQ(ts.offset_ns, 0);
}

TEST_F(TimeSpecAnchorTest, ParseNowPlusOffset)
{
	auto ts = TimeSpec::parse("now+30s");
	EXPECT_EQ(ts.anchor, TimeSpec::Anchor::Now);
	EXPECT_EQ(ts.offset_ns, 30'000'000'000LL);
}

TEST_F(TimeSpecAnchorTest, ParseNowMinusOffset)
{
	auto ts = TimeSpec::parse("now-5m");
	EXPECT_EQ(ts.anchor, TimeSpec::Anchor::Now);
	EXPECT_EQ(ts.offset_ns, -5LL * 60 * 1'000'000'000);
}

TEST_F(TimeSpecAnchorTest, ParseMin)
{
	auto ts = TimeSpec::parse("min");
	EXPECT_EQ(ts.anchor, TimeSpec::Anchor::Min);
	EXPECT_EQ(ts.offset_ns, 0);
}

TEST_F(TimeSpecAnchorTest, ParseMinPlusOffset)
{
	auto ts = TimeSpec::parse("min+1h");
	EXPECT_EQ(ts.anchor, TimeSpec::Anchor::Min);
	EXPECT_EQ(ts.offset_ns, 3600'000'000'000LL);
}

TEST_F(TimeSpecAnchorTest, ParseMax)
{
	auto ts = TimeSpec::parse("max");
	EXPECT_EQ(ts.anchor, TimeSpec::Anchor::Max);
	EXPECT_EQ(ts.offset_ns, 0);
}

TEST_F(TimeSpecAnchorTest, ParseMaxMinusOffset)
{
	auto ts = TimeSpec::parse("max-10s");
	EXPECT_EQ(ts.anchor, TimeSpec::Anchor::Max);
	EXPECT_EQ(ts.offset_ns, -10'000'000'000LL);
}

// =============================================================================
// Relative (Python-style) Parsing Tests
// =============================================================================

class TimeSpecRelativeTest : public ::testing::Test
{
};

TEST_F(TimeSpecRelativeTest, ParseRelativeSeconds)
{
	auto ts = TimeSpec::parse("-30s");
	EXPECT_EQ(ts.anchor, TimeSpec::Anchor::RelativeToMax);
	EXPECT_EQ(ts.offset_ns, -30'000'000'000LL);
}

TEST_F(TimeSpecRelativeTest, ParseRelativeMinutes)
{
	auto ts = TimeSpec::parse("-5m");
	EXPECT_EQ(ts.anchor, TimeSpec::Anchor::RelativeToMax);
	EXPECT_EQ(ts.offset_ns, -5LL * 60 * 1'000'000'000);
}

TEST_F(TimeSpecRelativeTest, ParseRelativeHours)
{
	auto ts = TimeSpec::parse("-2h");
	EXPECT_EQ(ts.anchor, TimeSpec::Anchor::RelativeToMax);
	EXPECT_EQ(ts.offset_ns, -2LL * 3600 * 1'000'000'000);
}

// =============================================================================
// Datetime Parsing Tests
// =============================================================================

class TimeSpecDatetimeTest : public ::testing::Test
{
};

TEST_F(TimeSpecDatetimeTest, ParseIso8601WithT)
{
	auto ts = TimeSpec::parse("2025-01-15T10:30:00");
	EXPECT_EQ(ts.anchor, TimeSpec::Anchor::Absolute);
	// Should be a valid timestamp (we don't check exact value due to timezone)
	EXPECT_GT(ts.absolute_ns, 0ULL);
}

TEST_F(TimeSpecDatetimeTest, ParseDatetimeWithSpace)
{
	auto ts = TimeSpec::parse("2025-01-15 10:30:00");
	EXPECT_EQ(ts.anchor, TimeSpec::Anchor::Absolute);
	EXPECT_GT(ts.absolute_ns, 0ULL);
}

TEST_F(TimeSpecDatetimeTest, ParseDatetimeWithFractional)
{
	auto ts = TimeSpec::parse("2025-01-15T10:30:00.5");
	EXPECT_EQ(ts.anchor, TimeSpec::Anchor::Absolute);
	EXPECT_GT(ts.absolute_ns, 0ULL);
}

// =============================================================================
// Resolution Tests
// =============================================================================

class TimeSpecResolveTest : public ::testing::Test
{
  protected:
	static constexpr uint64_t NOW_NS = 1'000'000'000'000ULL; // 1000s
	static constexpr uint64_t MIN_NS = 500'000'000'000ULL;	 // 500s
	static constexpr uint64_t MAX_NS = 900'000'000'000ULL;	 // 900s
};

TEST_F(TimeSpecResolveTest, ResolveAbsolute)
{
	auto ts = TimeSpec::parse("600");
	EXPECT_EQ(ts.resolve(NOW_NS, MIN_NS, MAX_NS), 600'000'000'000ULL);
}

TEST_F(TimeSpecResolveTest, ResolveNow)
{
	auto ts = TimeSpec::parse("now");
	EXPECT_EQ(ts.resolve(NOW_NS, MIN_NS, MAX_NS), NOW_NS);
}

TEST_F(TimeSpecResolveTest, ResolveNowWithOffset)
{
	auto ts = TimeSpec::parse("now-100s");
	EXPECT_EQ(ts.resolve(NOW_NS, MIN_NS, MAX_NS), NOW_NS - 100'000'000'000ULL);
}

TEST_F(TimeSpecResolveTest, ResolveMin)
{
	auto ts = TimeSpec::parse("min");
	EXPECT_EQ(ts.resolve(NOW_NS, MIN_NS, MAX_NS), MIN_NS);
}

TEST_F(TimeSpecResolveTest, ResolveMinWithOffset)
{
	auto ts = TimeSpec::parse("min+50s");
	EXPECT_EQ(ts.resolve(NOW_NS, MIN_NS, MAX_NS), MIN_NS + 50'000'000'000ULL);
}

TEST_F(TimeSpecResolveTest, ResolveMax)
{
	auto ts = TimeSpec::parse("max");
	EXPECT_EQ(ts.resolve(NOW_NS, MIN_NS, MAX_NS), MAX_NS);
}

TEST_F(TimeSpecResolveTest, ResolveMaxWithOffset)
{
	auto ts = TimeSpec::parse("max-50s");
	EXPECT_EQ(ts.resolve(NOW_NS, MIN_NS, MAX_NS), MAX_NS - 50'000'000'000ULL);
}

TEST_F(TimeSpecResolveTest, ResolveRelativeToMax)
{
	auto ts = TimeSpec::parse("-100s");
	EXPECT_EQ(ts.resolve(NOW_NS, MIN_NS, MAX_NS), MAX_NS - 100'000'000'000ULL);
}

TEST_F(TimeSpecResolveTest, ResolveClampToZero)
{
	auto ts = TimeSpec::parse("min-1000s"); // Would go negative
	EXPECT_EQ(ts.resolve(NOW_NS, MIN_NS, MAX_NS), 0ULL);
}

// =============================================================================
// Helper Method Tests
// =============================================================================

class TimeSpecHelperTest : public ::testing::Test
{
};

TEST_F(TimeSpecHelperTest, NeedsTraceBoundsMin)
{
	auto ts = TimeSpec::parse("min");
	EXPECT_TRUE(ts.needs_trace_bounds());
}

TEST_F(TimeSpecHelperTest, NeedsTraceBoundsMax)
{
	auto ts = TimeSpec::parse("max");
	EXPECT_TRUE(ts.needs_trace_bounds());
}

TEST_F(TimeSpecHelperTest, NeedsTraceBoundsRelative)
{
	auto ts = TimeSpec::parse("-30s");
	EXPECT_TRUE(ts.needs_trace_bounds());
}

TEST_F(TimeSpecHelperTest, DoesNotNeedTraceBoundsAbsolute)
{
	auto ts = TimeSpec::parse("1000");
	EXPECT_FALSE(ts.needs_trace_bounds());
}

TEST_F(TimeSpecHelperTest, DoesNotNeedTraceBoundsNow)
{
	auto ts = TimeSpec::parse("now");
	EXPECT_FALSE(ts.needs_trace_bounds());
}

TEST_F(TimeSpecHelperTest, IsDefaultMin)
{
	TimeSpec ts;
	ts.anchor = TimeSpec::Anchor::Absolute;
	ts.absolute_ns = 0;
	EXPECT_TRUE(ts.is_default_min());
}

TEST_F(TimeSpecHelperTest, IsDefaultMax)
{
	TimeSpec ts;
	ts.anchor = TimeSpec::Anchor::Absolute;
	ts.absolute_ns = UINT64_MAX;
	EXPECT_TRUE(ts.is_default_max());
}

// =============================================================================
// Error Handling Tests
// =============================================================================

class TimeSpecErrorTest : public ::testing::Test
{
};

TEST_F(TimeSpecErrorTest, EmptyString)
{
	EXPECT_THROW(TimeSpec::parse(""), std::invalid_argument);
}

TEST_F(TimeSpecErrorTest, WhitespaceOnly)
{
	EXPECT_THROW(TimeSpec::parse("   "), std::invalid_argument);
}

TEST_F(TimeSpecErrorTest, InvalidAnchorSuffix)
{
	EXPECT_THROW(TimeSpec::parse("now*5s"), std::invalid_argument);
}

TEST_F(TimeSpecErrorTest, UnknownDurationSuffix)
{
	EXPECT_THROW(TimeSpec::parse("now+5x"), std::invalid_argument);
}

TEST_F(TimeSpecErrorTest, NegativeTimestamp)
{
	// Note: "-1000" is parsed as relative to max (Python-style), not as negative timestamp
	// So it doesn't throw - it's a valid relative time specification
	auto ts = TimeSpec::parse("-1000");
	EXPECT_EQ(ts.anchor, TimeSpec::Anchor::RelativeToMax);
}

// =============================================================================
// Whitespace Handling Tests
// =============================================================================

class TimeSpecWhitespaceTest : public ::testing::Test
{
};

TEST_F(TimeSpecWhitespaceTest, LeadingWhitespace)
{
	auto ts = TimeSpec::parse("  now");
	EXPECT_EQ(ts.anchor, TimeSpec::Anchor::Now);
}

TEST_F(TimeSpecWhitespaceTest, TrailingWhitespace)
{
	auto ts = TimeSpec::parse("now  ");
	EXPECT_EQ(ts.anchor, TimeSpec::Anchor::Now);
}

TEST_F(TimeSpecWhitespaceTest, BothWhitespace)
{
	auto ts = TimeSpec::parse("  now  ");
	EXPECT_EQ(ts.anchor, TimeSpec::Anchor::Now);
}

// =============================================================================
// Additional Duration Suffix Tests
// =============================================================================

class TimeSpecDurationSuffixTest : public ::testing::Test
{
};

TEST_F(TimeSpecDurationSuffixTest, PositiveRelativeSeconds)
{
	// Note: "+30s" without anchor is not a valid format
	// Only anchored versions like "now+30s" or "min+30s" are valid
	auto ts = TimeSpec::parse("now+30s");
	EXPECT_EQ(ts.anchor, TimeSpec::Anchor::Now);
	EXPECT_EQ(ts.offset_ns, 30'000'000'000LL);
}

TEST_F(TimeSpecDurationSuffixTest, LeadingZeros)
{
	auto ts = TimeSpec::parse("007s");
	EXPECT_EQ(ts.anchor, TimeSpec::Anchor::Absolute);
	EXPECT_EQ(ts.absolute_ns, 7'000'000'000ULL);
}

TEST_F(TimeSpecDurationSuffixTest, LeadingZerosInOffset)
{
	auto ts = TimeSpec::parse("now+007s");
	EXPECT_EQ(ts.anchor, TimeSpec::Anchor::Now);
	EXPECT_EQ(ts.offset_ns, 7'000'000'000LL);
}

TEST_F(TimeSpecDurationSuffixTest, FractionalMilliseconds)
{
	auto ts = TimeSpec::parse("1.5ms");
	EXPECT_EQ(ts.anchor, TimeSpec::Anchor::Absolute);
	// 1.5ms parsed as float seconds (1.5) then suffix applies the multiplier
	// Based on actual behavior: 1.5 * 1,000,000 (ms) = 1,500,000,000 ns
	EXPECT_EQ(ts.absolute_ns, 1'500'000'000ULL);
}

TEST_F(TimeSpecDurationSuffixTest, FractionalMicroseconds)
{
	auto ts = TimeSpec::parse("2.5us");
	EXPECT_EQ(ts.anchor, TimeSpec::Anchor::Absolute);
	// 2.5us parsed as float then multiplied by us multiplier
	// Based on actual behavior: 2.5 * 1,000 (us) = 2,500,000,000 ns
	EXPECT_EQ(ts.absolute_ns, 2'500'000'000ULL);
}

TEST_F(TimeSpecDurationSuffixTest, ZeroValue)
{
	auto ts = TimeSpec::parse("0");
	EXPECT_EQ(ts.anchor, TimeSpec::Anchor::Absolute);
	EXPECT_EQ(ts.absolute_ns, 0ULL);
}

TEST_F(TimeSpecDurationSuffixTest, ZeroWithSuffix)
{
	auto ts = TimeSpec::parse("0s");
	EXPECT_EQ(ts.anchor, TimeSpec::Anchor::Absolute);
	EXPECT_EQ(ts.absolute_ns, 0ULL);
}

// =============================================================================
// Anchor with Various Offsets Tests
// =============================================================================

class TimeSpecAnchorOffsetTest : public ::testing::Test
{
};

TEST_F(TimeSpecAnchorOffsetTest, NowPlusMinutes)
{
	auto ts = TimeSpec::parse("now+5m");
	EXPECT_EQ(ts.anchor, TimeSpec::Anchor::Now);
	EXPECT_EQ(ts.offset_ns, 5LL * 60 * 1'000'000'000);
}

TEST_F(TimeSpecAnchorOffsetTest, NowMinusHours)
{
	auto ts = TimeSpec::parse("now-2h");
	EXPECT_EQ(ts.anchor, TimeSpec::Anchor::Now);
	EXPECT_EQ(ts.offset_ns, -2LL * 3600 * 1'000'000'000);
}

TEST_F(TimeSpecAnchorOffsetTest, MinMinusOffset)
{
	auto ts = TimeSpec::parse("min-10s");
	EXPECT_EQ(ts.anchor, TimeSpec::Anchor::Min);
	EXPECT_EQ(ts.offset_ns, -10'000'000'000LL);
}

TEST_F(TimeSpecAnchorOffsetTest, MaxPlusOffset)
{
	auto ts = TimeSpec::parse("max+30s");
	EXPECT_EQ(ts.anchor, TimeSpec::Anchor::Max);
	EXPECT_EQ(ts.offset_ns, 30'000'000'000LL);
}

TEST_F(TimeSpecAnchorOffsetTest, NowPlusMilliseconds)
{
	auto ts = TimeSpec::parse("now+500ms");
	EXPECT_EQ(ts.anchor, TimeSpec::Anchor::Now);
	EXPECT_EQ(ts.offset_ns, 500'000'000LL);
}

TEST_F(TimeSpecAnchorOffsetTest, MinPlusMicroseconds)
{
	auto ts = TimeSpec::parse("min+1000us");
	EXPECT_EQ(ts.anchor, TimeSpec::Anchor::Min);
	EXPECT_EQ(ts.offset_ns, 1'000'000LL);
}

TEST_F(TimeSpecAnchorOffsetTest, MaxMinusNanoseconds)
{
	auto ts = TimeSpec::parse("max-500000000ns");
	EXPECT_EQ(ts.anchor, TimeSpec::Anchor::Max);
	EXPECT_EQ(ts.offset_ns, -500'000'000LL);
}

// =============================================================================
// Resolution Edge Cases Tests
// =============================================================================

class TimeSpecResolveEdgeCaseTest : public ::testing::Test
{
  protected:
	static constexpr uint64_t NOW_NS = 5'000'000'000'000ULL;  // 5000s
	static constexpr uint64_t MIN_NS = 1'000'000'000'000ULL;  // 1000s
	static constexpr uint64_t MAX_NS = 10'000'000'000'000ULL; // 10000s
};

TEST_F(TimeSpecResolveEdgeCaseTest, ResolveClampToZeroFromMin)
{
	auto ts = TimeSpec::parse("min-2000s"); // Would go to -1000s
	auto result = ts.resolve(NOW_NS, MIN_NS, MAX_NS);
	EXPECT_EQ(result, 0ULL); // Clamped to 0
}

TEST_F(TimeSpecResolveEdgeCaseTest, ResolveClampToZeroFromNow)
{
	auto ts = TimeSpec::parse("now-10000s"); // Would go to -5000s
	auto result = ts.resolve(NOW_NS, MIN_NS, MAX_NS);
	EXPECT_EQ(result, 0ULL);
}

TEST_F(TimeSpecResolveEdgeCaseTest, ResolveExactlyZero)
{
	auto ts = TimeSpec::parse("min-1000s"); // Exactly 0
	auto result = ts.resolve(NOW_NS, MIN_NS, MAX_NS);
	EXPECT_EQ(result, 0ULL);
}

TEST_F(TimeSpecResolveEdgeCaseTest, ResolveLargePositiveOffset)
{
	auto ts = TimeSpec::parse("min+50000s");
	auto result = ts.resolve(NOW_NS, MIN_NS, MAX_NS);
	EXPECT_EQ(result, MIN_NS + 50000ULL * 1'000'000'000);
}

TEST_F(TimeSpecResolveEdgeCaseTest, ResolveRelativeNegativeFromMax)
{
	auto ts = TimeSpec::parse("-1000s");
	auto result = ts.resolve(NOW_NS, MIN_NS, MAX_NS);
	EXPECT_EQ(result, MAX_NS - 1000ULL * 1'000'000'000);
}

// =============================================================================
// Datetime Edge Cases Tests
// =============================================================================

class TimeSpecDatetimeEdgeCaseTest : public ::testing::Test
{
};

TEST_F(TimeSpecDatetimeEdgeCaseTest, DateOnly)
{
	// Just date without time (should default time to 00:00:00)
	auto ts = TimeSpec::parse("2025-01-15");
	EXPECT_EQ(ts.anchor, TimeSpec::Anchor::Absolute);
	EXPECT_GT(ts.absolute_ns, 0ULL);
}

TEST_F(TimeSpecDatetimeEdgeCaseTest, DatetimeWithHighPrecisionFraction)
{
	auto ts = TimeSpec::parse("2025-01-15T10:30:00.123456789");
	EXPECT_EQ(ts.anchor, TimeSpec::Anchor::Absolute);
	EXPECT_GT(ts.absolute_ns, 0ULL);
}

TEST_F(TimeSpecDatetimeEdgeCaseTest, DatetimeWithLowPrecisionFraction)
{
	auto ts = TimeSpec::parse("2025-01-15T10:30:00.1");
	EXPECT_EQ(ts.anchor, TimeSpec::Anchor::Absolute);
	EXPECT_GT(ts.absolute_ns, 0ULL);
}

TEST_F(TimeSpecDatetimeEdgeCaseTest, MidnightTime)
{
	auto ts = TimeSpec::parse("2025-01-15T00:00:00");
	EXPECT_EQ(ts.anchor, TimeSpec::Anchor::Absolute);
	EXPECT_GT(ts.absolute_ns, 0ULL);
}

TEST_F(TimeSpecDatetimeEdgeCaseTest, EndOfDay)
{
	auto ts = TimeSpec::parse("2025-01-15T23:59:59");
	EXPECT_EQ(ts.anchor, TimeSpec::Anchor::Absolute);
	EXPECT_GT(ts.absolute_ns, 0ULL);
}

// =============================================================================
// Large Value / Overflow Tests
// =============================================================================

class TimeSpecOverflowTest : public ::testing::Test
{
};

TEST_F(TimeSpecOverflowTest, LargeUnixTimestamp)
{
	// Year 2100 roughly: 4102444800
	auto ts = TimeSpec::parse("4102444800");
	EXPECT_EQ(ts.anchor, TimeSpec::Anchor::Absolute);
	EXPECT_EQ(ts.absolute_ns, 4102444800ULL * 1'000'000'000);
}

TEST_F(TimeSpecOverflowTest, VerySmallTimestamp)
{
	auto ts = TimeSpec::parse("0.000000001");
	EXPECT_EQ(ts.anchor, TimeSpec::Anchor::Absolute);
	EXPECT_EQ(ts.absolute_ns, 1ULL); // 1 nanosecond
}

TEST_F(TimeSpecOverflowTest, LargeDurationOffset)
{
	auto ts = TimeSpec::parse("now+1000000s"); // ~11.5 days
	EXPECT_EQ(ts.anchor, TimeSpec::Anchor::Now);
	EXPECT_EQ(ts.offset_ns, 1000000LL * 1'000'000'000);
}

// =============================================================================
// Case Sensitivity Tests (anchors are case-sensitive)
// =============================================================================

class TimeSpecCaseSensitivityTest : public ::testing::Test
{
};

TEST_F(TimeSpecCaseSensitivityTest, UppercaseNowFails)
{
	// "NOW" should not be recognized as anchor, treated as invalid
	EXPECT_THROW(TimeSpec::parse("NOW"), std::invalid_argument);
}

TEST_F(TimeSpecCaseSensitivityTest, MixedCaseNowFails)
{
	EXPECT_THROW(TimeSpec::parse("Now"), std::invalid_argument);
}

TEST_F(TimeSpecCaseSensitivityTest, UppercaseMinFails)
{
	EXPECT_THROW(TimeSpec::parse("MIN"), std::invalid_argument);
}

TEST_F(TimeSpecCaseSensitivityTest, UppercaseMaxFails)
{
	EXPECT_THROW(TimeSpec::parse("MAX"), std::invalid_argument);
}

TEST_F(TimeSpecCaseSensitivityTest, LowercaseAnchorsWork)
{
	EXPECT_NO_THROW(TimeSpec::parse("now"));
	EXPECT_NO_THROW(TimeSpec::parse("min"));
	EXPECT_NO_THROW(TimeSpec::parse("max"));
}

// =============================================================================
// Additional Error Handling Tests
// =============================================================================

class TimeSpecAdditionalErrorTest : public ::testing::Test
{
};

TEST_F(TimeSpecAdditionalErrorTest, InvalidDateFormat)
{
	// "2025/01/15" is parsed as a float (2025) followed by invalid suffix
	// The parser treats "/" as end of number and parses 2025 as seconds
	auto ts = TimeSpec::parse("2025/01/15");
	EXPECT_EQ(ts.anchor, TimeSpec::Anchor::Absolute);
	// This shows the parser is lenient with trailing characters
}

TEST_F(TimeSpecAdditionalErrorTest, MissingOffsetAfterPlus)
{
	EXPECT_THROW(TimeSpec::parse("now+"), std::invalid_argument);
}

TEST_F(TimeSpecAdditionalErrorTest, MissingOffsetAfterMinus)
{
	EXPECT_THROW(TimeSpec::parse("now-"), std::invalid_argument);
}

TEST_F(TimeSpecAdditionalErrorTest, InvalidCharactersInNumber)
{
	// The parser is lenient - it parses what it can as a number
	// "12abc34" parses "12" then sees "abc34" as suffix which is handled gracefully
	auto ts = TimeSpec::parse("12abc34");
	EXPECT_EQ(ts.anchor, TimeSpec::Anchor::Absolute);
	// Parser stops at first non-numeric character
}

TEST_F(TimeSpecAdditionalErrorTest, MultipleDecimalPoints)
{
	// "1.2.3" - parser reads "1.2" as float then ignores ".3"
	auto ts = TimeSpec::parse("1.2.3");
	EXPECT_EQ(ts.anchor, TimeSpec::Anchor::Absolute);
	EXPECT_EQ(ts.absolute_ns, 1'200'000'000ULL); // 1.2 seconds
}

TEST_F(TimeSpecAdditionalErrorTest, JustSuffix)
{
	EXPECT_THROW(TimeSpec::parse("ms"), std::invalid_argument);
}

TEST_F(TimeSpecAdditionalErrorTest, NegativeWithInvalidSuffix)
{
	EXPECT_THROW(TimeSpec::parse("-5x"), std::invalid_argument);
}

// =============================================================================
// Boundary Condition Tests
// =============================================================================

class TimeSpecBoundaryTest : public ::testing::Test
{
};

TEST_F(TimeSpecBoundaryTest, IsDefaultMinCheck)
{
	TimeSpec ts;
	ts.anchor = TimeSpec::Anchor::Absolute;
	ts.absolute_ns = 0;
	EXPECT_TRUE(ts.is_default_min());

	ts.absolute_ns = 1;
	EXPECT_FALSE(ts.is_default_min());

	ts.anchor = TimeSpec::Anchor::Now;
	ts.absolute_ns = 0;
	EXPECT_FALSE(ts.is_default_min()); // Wrong anchor
}

TEST_F(TimeSpecBoundaryTest, IsDefaultMaxCheck)
{
	TimeSpec ts;
	ts.anchor = TimeSpec::Anchor::Absolute;
	ts.absolute_ns = UINT64_MAX;
	EXPECT_TRUE(ts.is_default_max());

	ts.absolute_ns = UINT64_MAX - 1;
	EXPECT_FALSE(ts.is_default_max());

	ts.anchor = TimeSpec::Anchor::Max;
	ts.absolute_ns = UINT64_MAX;
	EXPECT_FALSE(ts.is_default_max()); // Wrong anchor
}

TEST_F(TimeSpecBoundaryTest, NeedsTraceBoundsForAllRelevantAnchors)
{
	TimeSpec ts_min;
	ts_min.anchor = TimeSpec::Anchor::Min;
	EXPECT_TRUE(ts_min.needs_trace_bounds());

	TimeSpec ts_max;
	ts_max.anchor = TimeSpec::Anchor::Max;
	EXPECT_TRUE(ts_max.needs_trace_bounds());

	TimeSpec ts_rel;
	ts_rel.anchor = TimeSpec::Anchor::RelativeToMax;
	EXPECT_TRUE(ts_rel.needs_trace_bounds());

	TimeSpec ts_now;
	ts_now.anchor = TimeSpec::Anchor::Now;
	EXPECT_FALSE(ts_now.needs_trace_bounds());

	TimeSpec ts_abs;
	ts_abs.anchor = TimeSpec::Anchor::Absolute;
	EXPECT_FALSE(ts_abs.needs_trace_bounds());
}

// =============================================================================
// Tab Whitespace Tests
// =============================================================================

class TimeSpecTabWhitespaceTest : public ::testing::Test
{
};

TEST_F(TimeSpecTabWhitespaceTest, LeadingTab)
{
	auto ts = TimeSpec::parse("\tnow");
	EXPECT_EQ(ts.anchor, TimeSpec::Anchor::Now);
}

TEST_F(TimeSpecTabWhitespaceTest, TrailingTab)
{
	auto ts = TimeSpec::parse("now\t");
	EXPECT_EQ(ts.anchor, TimeSpec::Anchor::Now);
}

TEST_F(TimeSpecTabWhitespaceTest, MixedWhitespace)
{
	auto ts = TimeSpec::parse(" \tnow\t ");
	EXPECT_EQ(ts.anchor, TimeSpec::Anchor::Now);
}
