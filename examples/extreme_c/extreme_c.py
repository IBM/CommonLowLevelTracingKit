#!/usr/bin/python3
# Copyright (c) 2024, International Business Machines
# SPDX-License-Identifier: BSD-2-Clause-Patent

import sys
assert sys.version_info >= (3, 5), str(sys.version_info)
# %%


from itertools import product
from random import choice, randint, random


input = {
    "uint8_t": {
        "format": ["u", "d", "o", "x", "2u", "02u", "-3u", "4u", "x", "3x", "03x", "2X"],
        "value":  ["0", "1", "126", "127", "128", "254", "255"]
    },

    "int8_t": {
        "format": ["u", "d", "o", "x"],
        "value":  ["-128", "-127", "-126", "-1", "0", "1", "126", "127"]},

    "uint16_t": {
        "format": ["u", "d"],
        "value":  ["0", "1", "126", "127", "128"]},

    "int16_t": {
        "format": ["u", "d"],
        "value":  ["0", "1", "126", "127", "128"]},

    "uint32_t": {
        "format": ["u"],
        "value":  ["0", "1"]},

    "uint64_t": {
        "format": ["lu"],
        "value":  ["0", "1"]
    },

    "float": {
        "format": ["f", "e", "E", ".3e", "3e"],
        "value":  ["-1.0F", "0.0F", "1.0F", "1.3F", "3e-10F", "3.333e3F"]
    },
    "double": {
        "format": ["f", "e", "E", ".3e", "3e"],
        "value":  ["-1", "0", "1", "1.3", "3e-10", "3.333e3"]
    },
    "char*": {
        "format": ["s"],
        "value":  ["Some string argument.",
                   "Some string argument. Longer Argument so some more text here",
                   "Sort",
                   "A"*100,
                   "A"*200,
                   "A"*1000,
                   ]
    },
}

n_tp = 1000


def get_tracepoints() -> str:
    tracepoints = []
    for tracepoint_index in range(n_tp):
        n = randint(0, 9)
        if n:
            formats = []
            vars = []
            for arg_index in range(n):
                type, body = choice(list(input.items()))
                (type_formats, type_values) = body.values()
                type_format = choice(type_formats)
                type_value = choice(type_values)

                if type == "char*":
                    vars.append(f"\t{type} arg{arg_index} = \"{type_value}\";")
                else:
                    vars.append(f"\t{type} arg{arg_index} = {type_value};")

                formats.append(
                    f"{arg_index}: %%{type_format} ({type})\\\"{type_value}\\\" =?= %{type_format}")

            formats = " ".join(formats)
            args = ", ".join(f"arg{arg_index}" for arg_index in range(n))

            tracepoints.append("{")
            tracepoints += vars
            tracepoints.append(
                f"\tCLLTK_TRACEPOINT(EXTREME_C,\"{tracepoint_index:010d} format: {formats}\", {args});")
            tracepoints.append("}")
        else:
            tracepoints.append("{")
            tracepoints.append(
                f"\tCLLTK_TRACEPOINT(EXTREME_C,\"{tracepoint_index:010d} no args\");")
            tracepoints.append("}")
    return "\n\t\t".join(tracepoints)


with open("extreme_c.gen.c", "w") as fh:
    fh.write(f'''
# include "CommonLowLevelTracingKit/tracing.h"
# include <stdio.h>
# include <stdint.h>
# include <stdlib.h>

CLLTK_TRACEBUFFER(EXTREME_C, 100*4096)

int main(int argc, char *argv[])
{{
    int LOOPS = 1;
    if (argc == 2)
    {{
        _CLLTK_PRAGMA_DIAG(push)
        _CLLTK_PRAGMA_DIAG_CLANG(ignored "-Wunsafe-buffer-usage")
        char *a = argv[1];
        _CLLTK_PRAGMA_DIAG(pop)
        LOOPS = atoi(a);
    }}
    printf("timestamp: %s\\n", __TIMESTAMP__);
    printf("total %lu\\n", (size_t)LOOPS * (size_t){n_tp:d} + 1);
    
    CLLTK_TRACEPOINT(EXTREME_C, "timestamp: %s", __TIMESTAMP__);
    CLLTK_TRACEPOINT(EXTREME_C, "total %lu", (size_t)LOOPS * (size_t){n_tp:d} + 1);
    for(int i = 0; i < LOOPS; i++)
    {{    
    { get_tracepoints() }
    }}
    return 0;
}}
''')
