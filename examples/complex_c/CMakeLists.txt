# Copyright (c) 2024, International Business Machines
# SPDX-License-Identifier: BSD-2-Clause-Patent


add_executable(example-complex_c
    complex_c.c
)

target_link_libraries(example-complex_c
PRIVATE
    clltk_tracing
    clltk_example_flags
)

add_dependencies(example-complex_c
    clean_up_traces
)
