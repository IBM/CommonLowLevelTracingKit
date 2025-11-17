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

class tracing : public testing::Test
{
  public:
	static inline const char *root = ".";

  protected:
	void SetUp() override { system("find ./ -name \"*.clltk_trace\" | xargs -I{} rm {}"); }
};

#define TB CLLTK_TRACEBUFFER_MACRO_VALUE(tracing)
CLLTK_TRACEBUFFER(TB, 1024);
#define TP(...) CLLTK_TRACEPOINT(TB, __VA_ARGS__)

#define Data(_Str_) _Str_, sizeof(_Str_)
TEST_F(tracing, static_tracing)
{
	_clltk_tracebuffer_deinit(_clltk_tracing_ptr);
	TP("hello");
}
