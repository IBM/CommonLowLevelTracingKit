// Copyright (c) 2024, International Business Machines
// SPDX-License-Identifier: BSD-2-Clause-Patent

#include "CommonLowLevelTracingKit/tracing/tracing.h"
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

	for (auto &tb : tbs)
		_clltk_tracebuffer_deinit(&tb);
}

TEST_F(api, _clltk_static_tracepoint_with_args)
{
	const static std::array<uint8_t, 10 * 1024> meta = {};
	_clltk_tracebuffer_handler_t hdl = {{"DUMMPY", 1024}, {meta.begin(), &meta.back()}, {}};
	_clltk_tracebuffer_init(&hdl);
	ASSERT_TRUE(hdl.runtime.tracebuffer);
	// test stack increase
	const auto offset = _clltk_tracebuffer_add_to_stack(&hdl, meta.data(), (uint32_t)meta.size());
	ASSERT_TRUE(_CLLTK_FILE_OFFSET_IS_STATIC(offset));
	static _clltk_argument_types_t types{};
	_clltk_static_tracepoint_with_args(&hdl, 0x101, __FILE__, __LINE__, &types,
									   "const char *const format");

	_clltk_tracebuffer_deinit(&hdl);
}

TEST_F(api, delete_tracebuffer)
{
	// nothing here, just to trigger SetUp()
}
