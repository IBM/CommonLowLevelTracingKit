#!/usr/bin/python3
# Copyright (c) 2024, International Business Machines
# SPDX-License-Identifier: BSD-2-Clause-Patent

import sys
assert sys.version_info >= (3, 5), str(sys.version_info)
# %%

import random

input = {
    "uint8_t": {
        "format": ["u", "d", "o", "x"],
        "value":  ["0", "1", "126", "127", "128", "254", "255"]
    },

    "int8_t": {
        "format": ["d", "x"],
        "value":  ["-128", "-127", "-126", "-1", "0", "1", "126", "127"]},

    "uint16_t": {
        "format": ["u", "d"],
        "value":  ["0", "1", "126", "127", "128"]},

    "int16_t": {
        "format": ["d", "x"],
        "value":  ["0", "1", "126", "127", "128"]},

    "uint32_t": {
        "format": ["u", "x"],
        "value":  ["0", "1"]},

    "int32_t": {
        "format": ["x"],
        "value":  ["0", "-1"]},

    "uint64_t": {
        "format": ["lu"],
        "value":  ["0", "1"]
    },

    "int64_t": {
        "format": ["ld", "lx"],
        "value":  ["0", "-1"]
    },

    "float": {
        "format": ["f", "e"],
        "value":  ["-1.0F", "0.0F", "1.0F", "1.3F", "3e-10F", "3.333e3F"]
    },
    "double": {
        "format": ["f", "e"],
        "value":  ["-1", "0", "1", "1.3", "3e-10", "3.333e3"]
    },
    "char[]": {
        "format": ["s"],
        "value":  [
                   "",
                   "A"*2,
                   "A"*10,
                   ]
    },
}
options = []
for key,body in input.items():
    for format in body['format']:
        for value in body['value']:
            options.append((key,format, value))
all_tests = []
def one_test(name: str, tests):
    name = name.replace('*', '')
    tests = '\n\t'.join(tests)
    all_tests.append( f'''TEST_F(TB, Test_{name})
{{
    {tests}
}}
''')
    
for n_args in range(1, 11):
    for i in range(0, min(n_args*10, 30), 10):
        call = []
        for j in range(i, i+10):
            format = ""
            arg_def = ""
            arg_call = ""
            args = random.sample(options, n_args)
            for x, (t, f, v) in enumerate(args):
                format += f" %{f}"
                if t == 'char[]':
                    arg_call += f', arg{x}'
                    arg_def += f'char arg{x}[] = "{v}";\n\t\t'
                else:
                    arg_call += f', arg{x}'
                    arg_def += f'{t} arg{x} = {v};\n\t\t'
            call.append(f'{{\n\t\t{arg_def}STATIC_TEST("{format}" {arg_call});\n\t}}')
    
        one_test(f'args_{n_args}_{i}',call)

with open("args_cpp.tests.cpp", "w") as fh:
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

#include "CommonLowLevelTracingKit/decoder/Tracebuffer.hpp"
#include "CommonLowLevelTracingKit/decoder/Tracepoint.hpp"
#include "CommonLowLevelTracingKit/tracing/tracing.h"
#include "gtest/gtest.h"

using namespace CommonLowLevelTracingKit::decoder;
using namespace std::string_literals;

#define TB CLLTK_TRACEBUFFER_MACRO_VALUE(args_cpp)
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

#define STATIC_TEST(FMT, ...)\\
	do {{\\
		char *expected_raw = nullptr;\\
		auto expected_size = asprintf(&expected_raw, FMT __VA_OPT__(, __VA_ARGS__));\\
		const auto expected = std::string_view{{expected_raw, (size_t)expected_size}};\\
		ASSERT_TRUE(expected_raw);\\
		TP(FMT __VA_OPT__(, __VA_ARGS__));\\
		ASSERT_GE(expected_size, 0);\\
		EXPECT_EQ(get_tp()->msg(), expected) << STR(FMT, ARG);\\
		free(expected_raw);\\
	}} while (false)

{"\n".join(all_tests)}
''')
