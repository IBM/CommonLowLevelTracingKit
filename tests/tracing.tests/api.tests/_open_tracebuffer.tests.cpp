// Copyright (c) 2024, International Business Machines
// SPDX-License-Identifier: BSD-2-Clause-Patent

#include "CommonLowLevelTracingKit/tracing.h"
#include "gtest/gtest.h"
#include <string>

extern "C" {
extern void file_reset();
}

class base_test : public testing::Test
{
  protected:
	void SetUp() override { file_reset(); }
};

class valid_paths : public base_test, public ::testing::WithParamInterface<std::string>
{
};

TEST_P(valid_paths, init_twice_valid)
{
	auto name = GetParam();
	// open first time

	_clltk_tracebuffer_handler_t handler_0 = {
		{name.data(), 1024}, {nullptr, nullptr}, {nullptr, 0}};
	_clltk_tracebuffer_init(&handler_0);
	EXPECT_NE(handler_0.runtime.tracebuffer, nullptr);

	// open second time
	_clltk_tracebuffer_handler_t handler_1 = {
		{name.data(), 1024}, {nullptr, nullptr}, {nullptr, 0}};
	_clltk_tracebuffer_init(&handler_1);
	EXPECT_EQ(handler_0.runtime.tracebuffer, handler_1.runtime.tracebuffer);

	_clltk_tracebuffer_deinit(&handler_0);
	_clltk_tracebuffer_deinit(&handler_1);
}

INSTANTIATE_TEST_CASE_P(open_tracebuffer, valid_paths, ::testing::Values("asd", "s"));
