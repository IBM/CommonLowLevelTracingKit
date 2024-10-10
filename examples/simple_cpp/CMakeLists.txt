# Copyright (c) 2024, International Business Machines
# SPDX-License-Identifier: BSD-2-Clause-Patent

add_executable(example-simple_cpp
    ./simple_cpp.cpp
)

target_link_libraries(example-simple_cpp
PRIVATE
    clltk_tracing
)


add_dependencies(example-simple_cpp
    clean_up_traces
)

set_target_properties(example-simple_cpp
    PROPERTIES
        CXX_STANDARD 17
        CXX_EXTENSIONS OFF
        CXX_STANDARD_REQUIRED ON
        COMPILE_WARNING_AS_ERROR ON
)
