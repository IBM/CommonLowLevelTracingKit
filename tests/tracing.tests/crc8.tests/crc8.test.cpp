// Copyright (c) 2024, International Business Machines
// SPDX-License-Identifier: BSD-2-Clause-Patent

#include "crc8/crc8.h"
#include "gtest/gtest.h"
#include <cstdint>
#include <stddef.h>
#include <string>
#include <tuple>
#include <type_traits>

class with_parameter : public ::testing::TestWithParam<std::tuple<std::string, uint8_t>>
{
};

TEST_P(with_parameter, crc8)
{
	const auto [str, c] = GetParam();
	const uint8_t *const data = reinterpret_cast<const uint8_t *>(str.c_str());
	const size_t size = str.size();
	const uint8_t crc = crc8_continue(0, data, size);
	EXPECT_EQ(crc, c);
}
TEST_P(with_parameter, crc8_continue)
{
	const auto [str, c] = GetParam();
	const uint8_t *const data = reinterpret_cast<const uint8_t *>(str.c_str());
	const size_t size = str.size();
	if ((size / 2) > 2) {
		const size_t size0 = (size / 2);
		const size_t size1 = size - size0;
		uint8_t crc = 0;
		crc = crc8_continue(crc, data, size0);
		crc = crc8_continue(crc, data + size0, size1);
		EXPECT_EQ(crc, c);
	} else {
		// do nothing
	}
}

#define Value(_STR_, _C_) \
	std::make_tuple<std::string, uint8_t>(std::string(_STR_, sizeof(_STR_) - 1), _C_)
INSTANTIATE_TEST_CASE_P(
	crc8, with_parameter,
	::testing::Values(
		Value("K5yLcCyPcglzklIeagvgmQqZf717PTSKI3dUCv2RJRQ6u65sM04ieTxK3psk3YQhcRkklG0XCpP", 'U'),
		Value(
			"X2wMAZ0exXPvTvN2EpAtw1OIWokYr7UcwVE8V9BkyQgInVHERdAtVniKX5dLB3zdRzqKX214y34xWWLBhHrYS",
			'Y'),
		Value("DX218iscxhdzjE9lQWLiTygfpLnpiYJ9RBqwHPQzYtJCkssXYRFtnVcb9mfL", 'W'),
		Value("uN6xoSDSiDrg08N9vhYFcBSABzV0R5B3yk8shm52D8H5FgYRXkuWZhbWY", 0x88),
		Value("YxjgJJQbaSgRFUP4w4I2j6Sk26H", 0xec),
		Value("7OvIQ6hUlJr3qN9XKSHjVh1y2B6yiPHt0v7zHIe6Ozn8AJUNuvzYUlHzy4T357U0M4jNJJBq", 'b'),
		Value("K31Cg3GxzkMelFiqS7r8zzufspqKmTv9LWw5HfkVKEIy359UZC4seCggif0jlwvgQETt7S10v45", 0x89),
		Value("KNLK8BswCyEceZ7lzJ", 'L'), Value("yFsWrkXuTRo3LrKGeD4sajGKhjADyAXpsAImqw5VEe", 0x98),
		Value("x1jVSggKLEP7H9Ellhq3omxTBxTlXc7dUBrA6B0Ue9OOHI8Njzwg80DIROUuHCi", 0x84)));
