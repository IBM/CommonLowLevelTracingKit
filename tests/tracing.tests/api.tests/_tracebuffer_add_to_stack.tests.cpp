// Copyright (c) 2024, International Business Machines
// SPDX-License-Identifier: BSD-2-Clause-Patent

#include "CommonLowLevelTracingKit/tracing.h"
#include "gtest/gtest.h"
#include <array>
#include <stdint.h>
#include <string>

extern "C" {
extern void file_reset();
}

class tracebuffer_add_to_stack : public ::testing::Test
{
  protected:
	void SetUp() override { file_reset(); }
};

TEST_F(tracebuffer_add_to_stack, simple)
{
	_clltk_tracebuffer_handler_t handler = {{"simple", 1024}, {}, 0};
	_clltk_handdler_open(&handler);
	auto &th = handler.tracebuffer;
	ASSERT_NE(th, nullptr);

	std::string str = "data set from simple";
	const auto offset = _clltk_tracebuffer_add_to_stack(th, str.data(), (uint32_t)str.size());
	EXPECT_GT(offset, 0);
	_clltk_handler_close(&handler);
}

TEST_F(tracebuffer_add_to_stack, twice_same)
{
	_clltk_tracebuffer_handler_t handler = {{"simple", 1024}, {}, 0};
	_clltk_handdler_open(&handler);
	auto &th = handler.tracebuffer;
	ASSERT_NE(th, nullptr);

	// add first time
	std::string str = "data set from twice same";
	const auto offset0 = _clltk_tracebuffer_add_to_stack(th, str.data(), (uint32_t)str.size());
	EXPECT_GT(offset0, 0);

	// add second time
	const auto offset1 = _clltk_tracebuffer_add_to_stack(th, str.data(), (uint32_t)str.size());
	EXPECT_EQ(offset0, offset1);

	_clltk_handler_close(&handler);
}

TEST_F(tracebuffer_add_to_stack, twice_different)
{
	_clltk_tracebuffer_handler_t handler = {{"simple", 1024}, {}, 0};
	_clltk_handdler_open(&handler);
	auto &th = handler.tracebuffer;
	ASSERT_NE(th, nullptr);

	// add first time
	std::string str0 = "fist data set from twice different";
	const auto offset0 = _clltk_tracebuffer_add_to_stack(th, str0.data(), (uint32_t)str0.size());
	EXPECT_GT(offset0, 0);

	// add second time
	std::string str1 = "second data set from twice different";
	const auto offset1 = _clltk_tracebuffer_add_to_stack(th, str1.data(), (uint32_t)str1.size());
	EXPECT_GT(offset1, 0);
	EXPECT_NE(offset0, offset1);

	_clltk_handler_close(&handler);
}

TEST_F(tracebuffer_add_to_stack, bigger_than_one_page)
{
	_clltk_tracebuffer_handler_t handler = {{"simple", 1024}, {}, 0};
	_clltk_handdler_open(&handler);
	auto &th = handler.tracebuffer;
	ASSERT_NE(th, nullptr);

	// add first time
	std::array<uint8_t, 1024 * 32> data{};
	const auto offset0 = _clltk_tracebuffer_add_to_stack(th, data.data(), (uint32_t)data.size());
	EXPECT_GT(offset0, 0);

	_clltk_handler_close(&handler);
}