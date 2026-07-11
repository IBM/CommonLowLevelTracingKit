# Copyright (c) 2026, International Business Machines
# SPDX-License-Identifier: BSD-2-Clause-Patent

add_executable(example-spans_c
    ./spans_c.c
)

target_link_libraries(example-spans_c
PRIVATE
    clltk_tracing
    clltk_example_flags
)

add_dependencies(example-spans_c
    clean_up_traces
)
