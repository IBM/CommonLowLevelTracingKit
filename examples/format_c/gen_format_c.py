#!/usr/bin/python3
# Copyright (c) 2024, International Business Machines
# SPDX-License-Identifier: BSD-2-Clause-Patent

import sys
assert sys.version_info >= (3, 5), str(sys.version_info)
# %%


from itertools import product


tracepoints = [
]


def gen(type: str, formats: list, values: list) -> None:
    if type == "char*":
        for format, value in product(formats, values):
            tracepoints.append(
                f'CLLTK_TRACEPOINT(GEN_FORMAT_C, "%%{format} with ({type})\\\"{value}\\\" = %{format}", ({type})"{value:s}");')
    else:
        for format, value in product(formats, values):
            tracepoints.append(
                f'CLLTK_TRACEPOINT(GEN_FORMAT_C, "%%{format} with ({type}){value} = %{format}", ({type}){value:s});')
  


gen("uint8_t",
    ["u", "d", "o", "x", "2u", "02u", "-3u", "4u"],
    ["-1", "0", "1", "126", "127", "128", "254", "255", "256", "257", "010"])

gen("int8_t",
    ["d", "d", "o", "x"],
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
    ["-1", "0", "1", "1.3", "3e-10", "3.333e3"]
    )
gen("char*",
    ["s"],
    ["ABCDEFG"]
    )


def get_tracepoints() -> str:
    return "\n\t".join(tracepoints)


with open("format_c.gen.c", "w") as fh:
    fh.write(f'''
# include "CommonLowLevelTracingKit/tracing.h"
# include <stdio.h>
# include <stdlib.h>

CLLTK_TRACEBUFFER(GEN_FORMAT_C, 16*4096)

int main(void)
{{
    { get_tracepoints() }
    return 0;
}}
''')
