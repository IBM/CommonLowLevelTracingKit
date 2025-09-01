// Copyright (c) 2024, International Business Machines
// SPDX-License-Identifier: BSD-2-Clause-Patent

#include <string>
#include <string_view>

#include "CommonLowLevelTracingKit/Decoder/Common.hpp"
#include "formatter.hpp"
#include "helper.hpp"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
using ::testing::StartsWith;

using namespace CommonLowLevelTracingKit::decoder::source::formatter;
using namespace CommonLowLevelTracingKit::decoder::exception;
using namespace std::string_literals;
using namespace std::string_view_literals;

struct Data {
	std::string_view format;
	std::string_view types;
	std::string_view raw_args;
	std::optional<std::string> expected;
	std::string_view file;
	size_t line;
};
#define DATA(...)                       \
	Data                                \
	{                                   \
		__VA_ARGS__, __FILE__, __LINE__ \
	}

inline std::ostream &operator<<(std::ostream &os, Data const &p)
{
	os << p.file << ":" << p.line;
	return os;
}

class decoder_formatter : public ::testing::TestWithParam<Data>
{
};

TEST_P(decoder_formatter, test_execution)
{
	const auto &param = GetParam();
	const auto types = span<char>(param.types);
	const auto raw_args = span<uint8_t>(param.raw_args);
	if (param.expected) {
		const auto msg = printf(param.format, types, raw_args);
		EXPECT_EQ(msg, param.expected) << param.file << ":" << param.line;
	} else {
		EXPECT_ANY_THROW(printf(param.format, types, raw_args)) << param.file << ":" << param.line;
	}
}

INSTANTIATE_TEST_SUITE_P(
	decoder_formatter_tests, decoder_formatter, // clang-format off
::testing::Values(
DATA("Hello World", "", "", "Hello World"),
DATA("", "", "", ""),
DATA("A\n0123456789", "", "", "A 0123456789"),
DATA("A\nA0123456789", "", "", "A A0123456789"),
DATA("\nA0123456789", "", "", " A0123456789"),
DATA("0123456789\n", "", "", "0123456789"),
DATA("\1", "", "", ""),
DATA("\n", "", "", ""),
DATA("\r", "", "", ""),
DATA("\t", "", "", ""),
DATA(" \1", "", "", " "),
DATA(" \n", "", "", " "),
DATA(" \r", "", "", " "),
DATA(" \t", "", "", " "),
DATA("H\1LMNOPQRSTUF", "", "", "H LMNOPQRSTUF"),
DATA("H\nLMNOPQRSTUF", "", "", "H LMNOPQRSTUF"),
DATA("H\rLMNOPQRSTUF", "", "", "H LMNOPQRSTUF"),
DATA("H\tLMNOPQRSTUF", "", "", "H LMNOPQRSTUF"),
DATA("%s", "", "", {}),
DATA("%d", "s", "ABCDEFGH", {}),
DATA("%d", "d", "ABCDEFG", {}),
DATA("%s", "c", "J", {}),
DATA("%c%c", "c", "AB", {}),
DATA("%c%c", "ccc", "AB", {}),
DATA("%c%c", "cc", "ABC", {}),
DATA("%c", "?", "A", {}),
DATA("%f", "d", "ABC", {})
));
												// clang-format on

TEST_F(decoder_formatter, exception)
{
	const auto format = "%s"sv;
	const auto types = span<char>("c");
	const auto raw_args = span<uint8_t>("A");
	try {
		printf(format, types, raw_args);
		FAIL() << "should throw";
	} catch (const FormattingFailed &e) {
		EXPECT_THAT(e.what(), StartsWith("FormattingFailed"));
	}
}