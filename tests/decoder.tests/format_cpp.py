#!/usr/bin/python3
# Copyright (c) 2024, International Business Machines
# SPDX-License-Identifier: BSD-2-Clause-Patent

from itertools import product
from collections import defaultdict
import sys
assert sys.version_info >= (3, 5), str(sys.version_info)
# %%

tests = defaultdict(list)


def gen(type: str, formats: list, values: list) -> None:
    if type == "char*":
        for format, value in product(formats, values):
            tests[type].append(
                f'STATIC_TEST({type}, "%{format}", &"{value:s}");')
    else:
        for format, value in product(formats, values):
            tests[type].append(
                f'STATIC_TEST({type}, "%{format}", {value:s});')


gen("uint8_t",
    ["u", "d", "o", "x", "2u", "02u", "-3u", "4u", ".03X"],
    ["-1", "0", "1", "126", "127", "128", "254", "255", "256", "257", "010"])

gen("int8_t",
    ["d", "d", "o", "x", ".0X", "3.3X"],
    ["-129", "-128", "-127", "-126", "-1", "0", "1", "126", "127", "128", "254", "255", "256", "257"])


gen("uint8_t",
    ["x", "3x", "03x", "2X"],
    ["-1", "0", "1", "126", "127", "128", "254", "255", "256", "257", "0x00", "0x10", "034"])

gen("uint16_t",
    ["u", "d"], ["-1", "0", "1", "126", "127",
                 "128", "2e16-2", "2e16-1", "2e16-0", "2e16+1", "2e16+2"])

gen("int16_t",
    ["d", "d", "i"],
    ["-(2e16+1)", "-(2e16+0)", "-(2e16-1)", "-(2e16-2)", "-(2e15+1)", "-(2e15+0)", "-(2e15-1)", "-(2e15-2)-1", "0", "1", "126", "127",
     "128", "2e15-2", "2e15-1", "2e15-0", "2e15+1", "2e15+2", "2e16-2", "2e16-1", "2e16-0", "2e16+1", "2e16+2"])

gen(
    "uint32_t",
    ["u", ".X", '9.X', '.0X'],
    ["-1", "0", "1", "2e32-2", "2e32-1", "2e32-0", "2e32+1"])

gen(
    "int32_t",
    ["d", "i"],
    ["-2e16-2", "-2e16-1", "-2e16-0", "-2e16+1", "-1", "0", "1", "2e16-2", "2e16-1", "2e16-0", "2e16+1", "2e32-2", "2e32-1", "2e32-0", "2e32+1"])

gen(
    "uint64_t",
    ["lu"],
    ["-1", "0", "1", "2e64-2", "2e64-1", "2e64-0", "2e64+1"])

gen(
    "int64_t",
    ["ld"],
    ["-2e32-2", "-2e32-1", "-2e32-0", "-2e32+1", "-1", "0", "1", "2e32-2", "2e32-1", "2e32-0", "2e32+1", "2e64-2", "2e64-1"])

gen("float",
    ["f", "e", "E", ".3e", "3e"],
    ["-1.0f", "0.0f", "1.0f", "1.3f", "3e-10f", "3.333e3f"]
    )
gen("double",
    ["f", "e", "E", ".3e", "3e"],
    ["-1", "0", "1", "1.3", "3e-10", "3.333e3"]
    )
gen("char*",
    ["s", "3s", "3.4s"],
    ["ABCDEFG", "", "\\x00", "A\\x03B", "\\x32", "Y\\x00X"]
    )


def get_tracepoints() -> str:
    def one_test(name: str, tests):
        name = name.replace('*', '')
        tests = '\n\t'.join(tests)
        return f'''TEST_F(decoder_extreme, Test_{name})
{{
    {tests}
}}
'''

    return "\n".join(one_test(k, v) for k, v in tests.items())


with open("format_cpp.tests.cpp", "w") as fh:
    fh.write(f'''
#include <stdio.h>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <stddef.h>
#include <string>
#include <string_view>
#include <vector>
#include "helper.hpp"

#include "CommonLowLevelTracingKit/Decoder/Tracebuffer.hpp"
#include "CommonLowLevelTracingKit/Decoder/Tracepoint.hpp"
#include "CommonLowLevelTracingKit/tracing.h"
#include "gtest/gtest.h"

using namespace CommonLowLevelTracingKit::decoder;
using namespace std::string_literals;

#define TB CLLTK_TRACEBUFFER_MACRO_VALUE(decoder_extreme)
constexpr size_t TB_SIZE = 1024;
CLLTK_TRACEBUFFER(TB, TB_SIZE)

static const std::string tb_name = STR(TB);
static const std::string file_path = trace_file(tb_name);
static void setup(){{
    SETUP(TB);
}}
static void clean_up(){{
    CLEANUP(TB);
}}

static TracepointPtr get_tp(){{

    auto tb = SnapTracebuffer::make(file_path);
    auto* tps = const_cast<TracepointCollection*>(&tb->tracepoints);
    return std::move(tps->back());
}}
class TB : public ::testing::Test
{{
	void SetUp() override {{setup();}}
 
	void TearDown() override {{clean_up();}}
}};

#define STATIC_TEST(TYPE, FMT, ARG)\\
	do {{\\
        const TYPE arg = (TYPE)(ARG); \\
		char *expected_raw = nullptr;\\
		auto expected_size = asprintf(&expected_raw, FMT, arg);\\
		const auto expected = std::string_view{{expected_raw, (size_t)expected_size}};\\
        std::cout << "test: \\"" << STR(FMT, ARG) << "\\" = \\"" << expected <<"\\"" << std::endl;\\
		ASSERT_TRUE(expected_raw);\\
		TP(FMT, arg);\\
		ASSERT_GE(expected_size, 0);\\
		EXPECT_EQ(get_tp()->msg(), expected) << STR(FMT, ARG);\\
		free(expected_raw);\\
	}} while (false)

{get_tracepoints()}
''')
# %%
