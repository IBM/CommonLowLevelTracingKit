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
	std::vector<_clltk_tracebuffer_handler_t> handlers{{{"first", 1024}, {}, 0},
													   {{"second", 1024}, {}, 0}};
	for (auto &hdl : handlers)
		_clltk_tracebuffer_init_handler(&hdl);

	for (auto &hdl : handlers) {
		for (size_t i = 0; i < 256; i++)
			_clltk_tracebuffer_add_to_stack(
				hdl.tracebuffer,
				Data("first stack entry with much much more data than any thing else"));

		_clltk_tracebuffer_add_to_stack(
			hdl.tracebuffer,
			Data("second stack entry with much much more data than any thing else"));
		for (size_t i = 0; i < 10; i++) {
			std::string str = "stack entry: " + std::to_string(i);
			_clltk_tracebuffer_add_to_stack(hdl.tracebuffer, str.data(), (uint32_t)str.size());
		}
	}

	for (auto &hdl : handlers)
		_clltk_tracebuffer_reset_handler(&hdl);
}

TEST_F(api, delete_tracebuffer)
{
	// nothing here, just to trigger SetUp()
}
