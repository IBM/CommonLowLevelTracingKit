// Copyright (c) 2024, International Business Machines
// SPDX-License-Identifier: BSD-2-Clause-Patent

#include <cstdint>
#include <string>
#include <string_view>

#include "filter.hpp"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

using namespace CommonLowLevelTracingKit::cmd::decode;

// =============================================================================
// Mock Tracepoint for Testing
// =============================================================================

// We need a way to create test tracepoints. Since Tracepoint is from decoder,
// we'll create a minimal mock that satisfies the filter interface.

class MockTracepoint
{
  public:
	uint64_t timestamp_ns{0};
	uint32_t m_pid{0};
	uint32_t m_tid{0};
	std::string m_msg;
	std::string m_file;

	uint32_t pid() const { return m_pid; }
	uint32_t tid() const { return m_tid; }
	std::string_view msg() const { return m_msg; }
	std::string_view file() const { return m_file; }
};

// =============================================================================
// Filter Configuration Tests
// =============================================================================

class FilterConfigTest : public ::testing::Test
{
};

TEST_F(FilterConfigTest, DefaultNoFilters)
{
	TracepointFilter filter;
	filter.configure();

	EXPECT_FALSE(filter.has_any_filter);
	EXPECT_FALSE(filter.has_time_filter);
	EXPECT_FALSE(filter.has_pid_filter);
	EXPECT_FALSE(filter.has_tid_filter);
	EXPECT_FALSE(filter.has_msg_filter);
	EXPECT_FALSE(filter.has_file_filter);
}

TEST_F(FilterConfigTest, TimeFilterDetected)
{
	TracepointFilter filter;
	filter.time_min = 1000;
	filter.configure();

	EXPECT_TRUE(filter.has_any_filter);
	EXPECT_TRUE(filter.has_time_filter);
}

TEST_F(FilterConfigTest, TimeFilterMaxDetected)
{
	TracepointFilter filter;
	filter.time_max = 1000;
	filter.configure();

	EXPECT_TRUE(filter.has_any_filter);
	EXPECT_TRUE(filter.has_time_filter);
}

TEST_F(FilterConfigTest, PidFilterDetected)
{
	TracepointFilter filter;
	filter.pids.insert(1234);
	filter.configure();

	EXPECT_TRUE(filter.has_any_filter);
	EXPECT_TRUE(filter.has_pid_filter);
}

TEST_F(FilterConfigTest, TidFilterDetected)
{
	TracepointFilter filter;
	filter.tids.insert(5678);
	filter.configure();

	EXPECT_TRUE(filter.has_any_filter);
	EXPECT_TRUE(filter.has_tid_filter);
}

TEST_F(FilterConfigTest, MsgSubstringFilterDetected)
{
	TracepointFilter filter;
	filter.set_msg_filter("error", false);
	filter.configure();

	EXPECT_TRUE(filter.has_any_filter);
	EXPECT_TRUE(filter.has_msg_filter);
	EXPECT_FALSE(filter.msg_use_regex);
}

TEST_F(FilterConfigTest, MsgRegexFilterDetected)
{
	TracepointFilter filter;
	filter.set_msg_filter("error.*", true);
	filter.configure();

	EXPECT_TRUE(filter.has_any_filter);
	EXPECT_TRUE(filter.has_msg_filter);
	EXPECT_TRUE(filter.msg_use_regex);
}

TEST_F(FilterConfigTest, FileSubstringFilterDetected)
{
	TracepointFilter filter;
	filter.set_file_filter("main.cpp", false);
	filter.configure();

	EXPECT_TRUE(filter.has_any_filter);
	EXPECT_TRUE(filter.has_file_filter);
	EXPECT_FALSE(filter.file_use_regex);
}

TEST_F(FilterConfigTest, FileRegexFilterDetected)
{
	TracepointFilter filter;
	filter.set_file_filter(".*\\.cpp$", true);
	filter.configure();

	EXPECT_TRUE(filter.has_any_filter);
	EXPECT_TRUE(filter.has_file_filter);
	EXPECT_TRUE(filter.file_use_regex);
}

TEST_F(FilterConfigTest, EmptyPatternIgnored)
{
	TracepointFilter filter;
	filter.set_msg_filter("", false);
	filter.set_file_filter("", true);
	filter.configure();

	EXPECT_FALSE(filter.has_any_filter);
	EXPECT_FALSE(filter.has_msg_filter);
	EXPECT_FALSE(filter.has_file_filter);
}

TEST_F(FilterConfigTest, MultipleFilters)
{
	TracepointFilter filter;
	filter.time_min = 1000;
	filter.pids.insert(1234);
	filter.set_msg_filter("error", false);
	filter.configure();

	EXPECT_TRUE(filter.has_any_filter);
	EXPECT_TRUE(filter.has_time_filter);
	EXPECT_TRUE(filter.has_pid_filter);
	EXPECT_TRUE(filter.has_msg_filter);
	EXPECT_FALSE(filter.has_tid_filter);
	EXPECT_FALSE(filter.has_file_filter);
}

// =============================================================================
// Filter Execution Tests using real Tracepoint
// =============================================================================

// Note: These tests require actual Tracepoint objects from the decoder library.
// For now, we test the configuration logic. Full filtering tests would require
// integration with the decoder library to create real Tracepoint instances.

class FilterExecutionTest : public ::testing::Test
{
  protected:
	TracepointFilter filter;

	void SetUp() override
	{
		// Reset filter for each test
		filter = TracepointFilter();
	}
};

// Test that configure() properly sets flags for combined filters
TEST_F(FilterExecutionTest, CombinedFilterFlags)
{
	filter.time_min = 100;
	filter.time_max = 200;
	filter.pids.insert(1);
	filter.pids.insert(2);
	filter.tids.insert(10);
	filter.set_msg_filter("test", false);
	filter.set_file_filter("src/", false);
	filter.configure();

	EXPECT_TRUE(filter.has_any_filter);
	EXPECT_TRUE(filter.has_time_filter);
	EXPECT_TRUE(filter.has_pid_filter);
	EXPECT_TRUE(filter.has_tid_filter);
	EXPECT_TRUE(filter.has_msg_filter);
	EXPECT_TRUE(filter.has_file_filter);
}

// Test PID set operations
TEST_F(FilterExecutionTest, PidSetContains)
{
	filter.pids.insert(100);
	filter.pids.insert(200);
	filter.pids.insert(300);

	EXPECT_TRUE(filter.pids.contains(100));
	EXPECT_TRUE(filter.pids.contains(200));
	EXPECT_TRUE(filter.pids.contains(300));
	EXPECT_FALSE(filter.pids.contains(400));
}

// Test TID set operations
TEST_F(FilterExecutionTest, TidSetContains)
{
	filter.tids.insert(1000);
	filter.tids.insert(2000);

	EXPECT_TRUE(filter.tids.contains(1000));
	EXPECT_TRUE(filter.tids.contains(2000));
	EXPECT_FALSE(filter.tids.contains(3000));
}

// Test time range boundaries
TEST_F(FilterExecutionTest, TimeRangeBoundaries)
{
	filter.time_min = 1000;
	filter.time_max = 2000;
	filter.configure();

	EXPECT_EQ(filter.time_min, 1000ULL);
	EXPECT_EQ(filter.time_max, 2000ULL);
	EXPECT_TRUE(filter.has_time_filter);
}

// Test default time range (no filtering)
TEST_F(FilterExecutionTest, DefaultTimeRange)
{
	filter.configure();

	EXPECT_EQ(filter.time_min, 0ULL);
	EXPECT_EQ(filter.time_max, UINT64_MAX);
	EXPECT_FALSE(filter.has_time_filter);
}

// =============================================================================
// Regex Compilation Tests
// =============================================================================

class FilterRegexTest : public ::testing::Test
{
};

TEST_F(FilterRegexTest, ValidMsgRegex)
{
	TracepointFilter filter;
	EXPECT_NO_THROW(filter.set_msg_filter("error.*warning", true));
}

TEST_F(FilterRegexTest, ValidFileRegex)
{
	TracepointFilter filter;
	EXPECT_NO_THROW(filter.set_file_filter(".*\\.(cpp|hpp)$", true));
}

TEST_F(FilterRegexTest, ComplexRegexPatterns)
{
	TracepointFilter filter;

	// Various regex patterns that should compile
	EXPECT_NO_THROW(filter.set_msg_filter("^[A-Z]+$", true));
	EXPECT_NO_THROW(filter.set_msg_filter("\\d{4}-\\d{2}-\\d{2}", true));
	EXPECT_NO_THROW(filter.set_msg_filter("(foo|bar|baz)", true));
	EXPECT_NO_THROW(filter.set_msg_filter("test\\s+message", true));
}

// =============================================================================
// Edge Cases
// =============================================================================

class FilterEdgeCaseTest : public ::testing::Test
{
};

TEST_F(FilterEdgeCaseTest, EmptyPidSet)
{
	TracepointFilter filter;
	filter.configure();

	EXPECT_TRUE(filter.pids.empty());
	EXPECT_FALSE(filter.has_pid_filter);
}

TEST_F(FilterEdgeCaseTest, SinglePid)
{
	TracepointFilter filter;
	filter.pids.insert(1);
	filter.configure();

	EXPECT_EQ(filter.pids.size(), 1UL);
	EXPECT_TRUE(filter.has_pid_filter);
}

TEST_F(FilterEdgeCaseTest, ManyPids)
{
	TracepointFilter filter;
	for (uint32_t i = 0; i < 1000; ++i) {
		filter.pids.insert(i);
	}
	filter.configure();

	EXPECT_EQ(filter.pids.size(), 1000UL);
	EXPECT_TRUE(filter.has_pid_filter);
}

TEST_F(FilterEdgeCaseTest, TimeMinEqualsMax)
{
	TracepointFilter filter;
	filter.time_min = 5000;
	filter.time_max = 5000;
	filter.configure();

	EXPECT_TRUE(filter.has_time_filter);
}

TEST_F(FilterEdgeCaseTest, SubstringInMiddle)
{
	TracepointFilter filter;
	filter.set_msg_filter("middle", false);
	filter.configure();

	EXPECT_EQ(filter.msg_substr, "middle");
	EXPECT_FALSE(filter.msg_use_regex);
}

// =============================================================================
// Invalid Regex Tests
// =============================================================================

class FilterInvalidRegexTest : public ::testing::Test
{
};

TEST_F(FilterInvalidRegexTest, InvalidMsgRegexThrows)
{
	TracepointFilter filter;
	// Unclosed parenthesis should throw
	EXPECT_THROW(filter.set_msg_filter("(unclosed", true), boost::regex_error);
}

TEST_F(FilterInvalidRegexTest, InvalidFileRegexThrows)
{
	TracepointFilter filter;
	// Unclosed bracket should throw
	EXPECT_THROW(filter.set_file_filter("[unclosed", true), boost::regex_error);
}

TEST_F(FilterInvalidRegexTest, InvalidQuantifierThrows)
{
	TracepointFilter filter;
	// Invalid quantifier
	EXPECT_THROW(filter.set_msg_filter("*invalid", true), boost::regex_error);
}

TEST_F(FilterInvalidRegexTest, InvalidRepetitionRange)
{
	TracepointFilter filter;
	// Invalid repetition range (max less than min)
	EXPECT_THROW(filter.set_msg_filter("a{5,2}", true), boost::regex_error);
}

// =============================================================================
// Filter Operator Tests with MockTracepoint
// =============================================================================

// Template-based filter for testing with MockTracepoint
template <typename T> bool apply_filter(const TracepointFilter &filter, const T &tp)
{
	// Fast path: no filters active
	if (!filter.has_any_filter)
		return true;

	// Time filter check
	if (filter.has_time_filter) {
		if (tp.timestamp_ns < filter.time_min || tp.timestamp_ns > filter.time_max)
			return false;
	}

	// PID filter check
	if (filter.has_pid_filter && !filter.pids.contains(tp.pid()))
		return false;

	// TID filter check
	if (filter.has_tid_filter && !filter.tids.contains(tp.tid()))
		return false;

	// Message filter check
	if (filter.has_msg_filter) {
		const auto msg = tp.msg();
		if (filter.msg_use_regex) {
			if (!boost::regex_search(msg.begin(), msg.end(), filter.msg_regex))
				return false;
		} else {
			if (msg.find(filter.msg_substr) == std::string_view::npos)
				return false;
		}
	}

	// File filter check
	if (filter.has_file_filter) {
		const auto file = tp.file();
		if (filter.file_use_regex) {
			if (!boost::regex_search(file.begin(), file.end(), filter.file_regex))
				return false;
		} else {
			if (file.find(filter.file_substr) == std::string_view::npos)
				return false;
		}
	}

	return true;
}

class FilterOperatorTest : public ::testing::Test
{
  protected:
	MockTracepoint create_tracepoint(uint64_t ts, uint32_t pid, uint32_t tid,
									 const std::string &msg, const std::string &file)
	{
		MockTracepoint tp;
		tp.timestamp_ns = ts;
		tp.m_pid = pid;
		tp.m_tid = tid;
		tp.m_msg = msg;
		tp.m_file = file;
		return tp;
	}
};

TEST_F(FilterOperatorTest, NoFilterPassesAll)
{
	TracepointFilter filter;
	filter.configure();

	auto tp = create_tracepoint(1000, 100, 200, "test message", "src/main.cpp");
	EXPECT_TRUE(apply_filter(filter, tp));
}

TEST_F(FilterOperatorTest, TimeFilterMinBoundary)
{
	TracepointFilter filter;
	filter.time_min = 1000;
	filter.configure();

	auto tp_below = create_tracepoint(999, 100, 200, "msg", "file");
	auto tp_exact = create_tracepoint(1000, 100, 200, "msg", "file");
	auto tp_above = create_tracepoint(1001, 100, 200, "msg", "file");

	EXPECT_FALSE(apply_filter(filter, tp_below));
	EXPECT_TRUE(apply_filter(filter, tp_exact));
	EXPECT_TRUE(apply_filter(filter, tp_above));
}

TEST_F(FilterOperatorTest, TimeFilterMaxBoundary)
{
	TracepointFilter filter;
	filter.time_max = 2000;
	filter.configure();

	auto tp_below = create_tracepoint(1999, 100, 200, "msg", "file");
	auto tp_exact = create_tracepoint(2000, 100, 200, "msg", "file");
	auto tp_above = create_tracepoint(2001, 100, 200, "msg", "file");

	EXPECT_TRUE(apply_filter(filter, tp_below));
	EXPECT_TRUE(apply_filter(filter, tp_exact));
	EXPECT_FALSE(apply_filter(filter, tp_above));
}

TEST_F(FilterOperatorTest, TimeFilterRange)
{
	TracepointFilter filter;
	filter.time_min = 1000;
	filter.time_max = 2000;
	filter.configure();

	auto tp_before = create_tracepoint(500, 100, 200, "msg", "file");
	auto tp_in_range = create_tracepoint(1500, 100, 200, "msg", "file");
	auto tp_after = create_tracepoint(2500, 100, 200, "msg", "file");

	EXPECT_FALSE(apply_filter(filter, tp_before));
	EXPECT_TRUE(apply_filter(filter, tp_in_range));
	EXPECT_FALSE(apply_filter(filter, tp_after));
}

TEST_F(FilterOperatorTest, PidFilterMatch)
{
	TracepointFilter filter;
	filter.pids.insert(100);
	filter.pids.insert(200);
	filter.configure();

	auto tp_match1 = create_tracepoint(1000, 100, 1, "msg", "file");
	auto tp_match2 = create_tracepoint(1000, 200, 1, "msg", "file");
	auto tp_no_match = create_tracepoint(1000, 300, 1, "msg", "file");

	EXPECT_TRUE(apply_filter(filter, tp_match1));
	EXPECT_TRUE(apply_filter(filter, tp_match2));
	EXPECT_FALSE(apply_filter(filter, tp_no_match));
}

TEST_F(FilterOperatorTest, TidFilterMatch)
{
	TracepointFilter filter;
	filter.tids.insert(1000);
	filter.tids.insert(2000);
	filter.configure();

	auto tp_match1 = create_tracepoint(1000, 1, 1000, "msg", "file");
	auto tp_match2 = create_tracepoint(1000, 1, 2000, "msg", "file");
	auto tp_no_match = create_tracepoint(1000, 1, 3000, "msg", "file");

	EXPECT_TRUE(apply_filter(filter, tp_match1));
	EXPECT_TRUE(apply_filter(filter, tp_match2));
	EXPECT_FALSE(apply_filter(filter, tp_no_match));
}

TEST_F(FilterOperatorTest, MsgSubstringMatch)
{
	TracepointFilter filter;
	filter.set_msg_filter("error", false);
	filter.configure();

	auto tp_start = create_tracepoint(1000, 1, 1, "error: something failed", "file");
	auto tp_middle = create_tracepoint(1000, 1, 1, "Found an error in processing", "file");
	auto tp_end = create_tracepoint(1000, 1, 1, "This is an error", "file");
	auto tp_no_match = create_tracepoint(1000, 1, 1, "Everything is fine", "file");

	EXPECT_TRUE(apply_filter(filter, tp_start));
	EXPECT_TRUE(apply_filter(filter, tp_middle));
	EXPECT_TRUE(apply_filter(filter, tp_end));
	EXPECT_FALSE(apply_filter(filter, tp_no_match));
}

TEST_F(FilterOperatorTest, MsgSubstringCaseSensitive)
{
	TracepointFilter filter;
	filter.set_msg_filter("Error", false);
	filter.configure();

	auto tp_exact = create_tracepoint(1000, 1, 1, "Error occurred", "file");
	auto tp_lower = create_tracepoint(1000, 1, 1, "error occurred", "file");
	auto tp_upper = create_tracepoint(1000, 1, 1, "ERROR occurred", "file");

	EXPECT_TRUE(apply_filter(filter, tp_exact));
	EXPECT_FALSE(apply_filter(filter, tp_lower));
	EXPECT_FALSE(apply_filter(filter, tp_upper));
}

TEST_F(FilterOperatorTest, MsgRegexMatch)
{
	TracepointFilter filter;
	filter.set_msg_filter("error.*failed", true);
	filter.configure();

	auto tp_match = create_tracepoint(1000, 1, 1, "error: operation failed", "file");
	auto tp_no_match = create_tracepoint(1000, 1, 1, "error occurred", "file");

	EXPECT_TRUE(apply_filter(filter, tp_match));
	EXPECT_FALSE(apply_filter(filter, tp_no_match));
}

TEST_F(FilterOperatorTest, MsgRegexPatternAlternation)
{
	TracepointFilter filter;
	filter.set_msg_filter("(error|warning|critical)", true);
	filter.configure();

	auto tp_error = create_tracepoint(1000, 1, 1, "An error occurred", "file");
	auto tp_warning = create_tracepoint(1000, 1, 1, "A warning was issued", "file");
	auto tp_critical = create_tracepoint(1000, 1, 1, "critical failure", "file");
	auto tp_info = create_tracepoint(1000, 1, 1, "info: all good", "file");

	EXPECT_TRUE(apply_filter(filter, tp_error));
	EXPECT_TRUE(apply_filter(filter, tp_warning));
	EXPECT_TRUE(apply_filter(filter, tp_critical));
	EXPECT_FALSE(apply_filter(filter, tp_info));
}

TEST_F(FilterOperatorTest, FileSubstringMatch)
{
	TracepointFilter filter;
	filter.set_file_filter("src/", false);
	filter.configure();

	auto tp_match = create_tracepoint(1000, 1, 1, "msg", "src/main.cpp");
	auto tp_no_match = create_tracepoint(1000, 1, 1, "msg", "test/test.cpp");

	EXPECT_TRUE(apply_filter(filter, tp_match));
	EXPECT_FALSE(apply_filter(filter, tp_no_match));
}

TEST_F(FilterOperatorTest, FileRegexMatch)
{
	TracepointFilter filter;
	filter.set_file_filter(".*\\.(cpp|hpp)$", true);
	filter.configure();

	auto tp_cpp = create_tracepoint(1000, 1, 1, "msg", "src/main.cpp");
	auto tp_hpp = create_tracepoint(1000, 1, 1, "msg", "include/header.hpp");
	auto tp_c = create_tracepoint(1000, 1, 1, "msg", "src/legacy.c");
	auto tp_h = create_tracepoint(1000, 1, 1, "msg", "include/legacy.h");

	EXPECT_TRUE(apply_filter(filter, tp_cpp));
	EXPECT_TRUE(apply_filter(filter, tp_hpp));
	EXPECT_FALSE(apply_filter(filter, tp_c));
	EXPECT_FALSE(apply_filter(filter, tp_h));
}

TEST_F(FilterOperatorTest, CombinedFiltersAllMustPass)
{
	TracepointFilter filter;
	filter.time_min = 1000;
	filter.time_max = 2000;
	filter.pids.insert(100);
	filter.set_msg_filter("important", false);
	filter.configure();

	// All conditions met
	auto tp_pass = create_tracepoint(1500, 100, 1, "important message", "file");
	EXPECT_TRUE(apply_filter(filter, tp_pass));

	// Wrong time
	auto tp_wrong_time = create_tracepoint(500, 100, 1, "important message", "file");
	EXPECT_FALSE(apply_filter(filter, tp_wrong_time));

	// Wrong PID
	auto tp_wrong_pid = create_tracepoint(1500, 200, 1, "important message", "file");
	EXPECT_FALSE(apply_filter(filter, tp_wrong_pid));

	// Wrong message
	auto tp_wrong_msg = create_tracepoint(1500, 100, 1, "trivial message", "file");
	EXPECT_FALSE(apply_filter(filter, tp_wrong_msg));
}

TEST_F(FilterOperatorTest, AllFiltersActive)
{
	TracepointFilter filter;
	filter.time_min = 1000;
	filter.time_max = 5000;
	filter.pids.insert(100);
	filter.tids.insert(200);
	filter.set_msg_filter("test", false);
	filter.set_file_filter("src", false);
	filter.configure();

	// All conditions met
	auto tp_pass = create_tracepoint(2000, 100, 200, "test message", "src/file.cpp");
	EXPECT_TRUE(apply_filter(filter, tp_pass));

	// Fail each condition individually
	auto tp_fail_time = create_tracepoint(6000, 100, 200, "test message", "src/file.cpp");
	auto tp_fail_pid = create_tracepoint(2000, 999, 200, "test message", "src/file.cpp");
	auto tp_fail_tid = create_tracepoint(2000, 100, 999, "test message", "src/file.cpp");
	auto tp_fail_msg = create_tracepoint(2000, 100, 200, "other message", "src/file.cpp");
	auto tp_fail_file = create_tracepoint(2000, 100, 200, "test message", "lib/file.cpp");

	EXPECT_FALSE(apply_filter(filter, tp_fail_time));
	EXPECT_FALSE(apply_filter(filter, tp_fail_pid));
	EXPECT_FALSE(apply_filter(filter, tp_fail_tid));
	EXPECT_FALSE(apply_filter(filter, tp_fail_msg));
	EXPECT_FALSE(apply_filter(filter, tp_fail_file));
}

TEST_F(FilterOperatorTest, EmptyMessageMatch)
{
	TracepointFilter filter;
	filter.set_msg_filter("", false); // Empty pattern should be ignored
	filter.configure();

	auto tp = create_tracepoint(1000, 1, 1, "", "file");
	EXPECT_TRUE(apply_filter(filter, tp)); // No filter active
}

TEST_F(FilterOperatorTest, SpecialRegexCharactersInSubstring)
{
	TracepointFilter filter;
	filter.set_msg_filter("[error]", false); // Should match literal "[error]"
	filter.configure();

	auto tp_match = create_tracepoint(1000, 1, 1, "msg [error] here", "file");
	auto tp_no_match = create_tracepoint(1000, 1, 1, "msg error here", "file");

	EXPECT_TRUE(apply_filter(filter, tp_match));
	EXPECT_FALSE(apply_filter(filter, tp_no_match));
}
