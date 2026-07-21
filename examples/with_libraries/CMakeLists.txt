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

# These two SHARED libraries expand the tracepoint macros inside a .so - the
# shape a consumer plugin/driver has. CLLTK_FATAL_LINKER_WARNINGS (from
# Sanitizers.cmake) fails the build on a "creating DT_TEXTREL" (or any other)
# linker warning here, instead of letting it scroll past unread; it is empty
# under sanitizers. The authoritative check is the packaging consumer test
# tests/packaging/test_consumer_shared_lib.py, which inspects the produced .so
# against the *installed* headers; this is the cheap build-time tripwire.

# example-with_libraries-shared ###########################
add_library(example-with_libraries-shared SHARED
    shared.cpp
)

target_link_libraries(example-with_libraries-shared
    PRIVATE
        clltk_tracing
        clltk_example_flags
)

target_link_options(example-with_libraries-shared
    PRIVATE
        ${CLLTK_FATAL_LINKER_WARNINGS}
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

target_link_options(example-with_libraries-dynamic
    PRIVATE
        ${CLLTK_FATAL_LINKER_WARNINGS}
)

target_link_directories(example-with_libraries-dynamic
    PRIVATE
        .
)

