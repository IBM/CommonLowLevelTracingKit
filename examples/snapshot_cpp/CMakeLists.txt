# Copyright (c) 2024, International Business Machines
# SPDX-License-Identifier: BSD-2-Clause-Patent

add_executable(example-snapshot_cpp
    ./snapshot.cpp
)

target_link_libraries(example-snapshot_cpp
PRIVATE
    clltk_snapshot_static
    clltk_example_flags
)

set_target_properties(example-snapshot_cpp
    PROPERTIES
        CXX_STANDARD 17
        CXX_EXTENSIONS OFF
        CXX_STANDARD_REQUIRED ON
        COMPILE_WARNING_AS_ERROR ON
)
