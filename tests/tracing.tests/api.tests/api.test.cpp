// Copyright (c) 2024, International Business Machines
// SPDX-License-Identifier: BSD-2-Clause-Patent

#include "CommonLowLevelTracingKit/tracing.h"
#include <gtest/gtest.h>
#include <stdint.h>
#include <stdlib.h>
#include <string>
#include <vector>

extern "C" {
extern void file_reset(const char *root);
}

class api : public testing::Test
{
  public:
	static inline const char *root = ".";

  protected:
	void SetUp() override { system("find ./ -name \"*.clltk_trace\" | xargs -I{} rm {}"); }
};

#define Data(_Str_) _Str_, sizeof(_Str_)
TEST_F(api, full_test)
{
	std::vector<_clltk_tracebuffer_handler_t> tbs{{{"first", 1024}, {0, 0}, {0, 0}},
												  {{"second", 1024}, {0, 0}, {0, 0}}};
	for (auto &handler : tbs)
		_clltk_tracebuffer_init(&handler);

	for (auto &tb : tbs) {
		for (size_t i = 0; i < 256; i++)
			_clltk_tracebuffer_add_to_stack(
				&tb, Data("first stack entry with much much more data than any thing else"));

		_clltk_tracebuffer_add_to_stack(
			&tb, Data("second stack entry with much much more data than any thing else"));
		for (size_t i = 0; i < 10; i++) {
			std::string str = "stack entry: " + std::to_string(i);
			_clltk_tracebuffer_add_to_stack(&tb, str.data(), (uint32_t)str.size());
		}
	}
}

TEST_F(api, delete_tracebuffer)
{
	// nothing here, just to trigger SetUp()
}
