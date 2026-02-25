# Copyright (c) 2024, International Business Machines
# SPDX-License-Identifier: BSD-2-Clause-Patent


add_executable(example-with_libraries
    with_libraries.cpp
)

target_link_libraries(example-with_libraries
    PRIVATE
        clltk_tracing
        clltk_example_flags
        example-with_libraries-static
        example-with_libraries-shared
        dl
)
add_dependencies(example-with_libraries
    example-with_libraries-dynamic
)

target_link_directories(example-with_libraries
    PRIVATE
        .
)

# example-with_libraries-static ###########################
add_library(example-with_libraries-static STATIC
    static.cpp
)

target_link_libraries(example-with_libraries-static
    PRIVATE
        clltk_tracing
        clltk_example_flags
)

target_link_directories(example-with_libraries-static
    PRIVATE
        .
)

# example-with_libraries-shared ###########################
add_library(example-with_libraries-shared SHARED
    shared.cpp
)

target_link_libraries(example-with_libraries-shared
    PRIVATE
        clltk_tracing
        clltk_example_flags
)

target_link_directories(example-with_libraries-shared
    PRIVATE
        .
)


# example-with_libraries-dynamic ###########################
add_library(example-with_libraries-dynamic SHARED
    dynamic.cpp
)

target_link_libraries(example-with_libraries-dynamic
    PRIVATE
        clltk_tracing
        clltk_example_flags
)

target_link_directories(example-with_libraries-dynamic
    PRIVATE
        .
)

