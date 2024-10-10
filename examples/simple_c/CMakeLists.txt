# Copyright (c) 2024, International Business Machines
# SPDX-License-Identifier: BSD-2-Clause-Patent

add_executable(example-simple_c
    ./simple_c.c
)

target_link_libraries(example-simple_c
PRIVATE
    clltk_tracing
)

add_dependencies(example-simple_c
    clean_up_traces
)
