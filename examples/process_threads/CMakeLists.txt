# Copyright (c) 2024, International Business Machines
# SPDX-License-Identifier: BSD-2-Clause-Patent

add_executable(example-process_threads
    ./process_threads.cpp
)

target_link_libraries(example-process_threads
PRIVATE
    clltk_tracing
    clltk_example_flags
    pthread
)

add_dependencies(example-process_threads
    clean_up_traces
)

set_target_properties(example-process_threads
    PROPERTIES
        CXX_STANDARD 17
        CXX_EXTENSIONS OFF
        CXX_STANDARD_REQUIRED ON
        COMPILE_WARNING_AS_ERROR ON
)
