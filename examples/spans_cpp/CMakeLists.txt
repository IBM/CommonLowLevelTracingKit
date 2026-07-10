# Copyright (c) 2026, International Business Machines
# SPDX-License-Identifier: BSD-2-Clause-Patent

add_executable(example-spans_cpp
    ./spans_cpp.cpp
)

target_link_libraries(example-spans_cpp
PRIVATE
    clltk_tracing
    clltk_example_flags
)

add_dependencies(example-spans_cpp
    clean_up_traces
)
