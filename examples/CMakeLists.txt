# Copyright (c) 2024, International Business Machines
# SPDX-License-Identifier: BSD-2-Clause-Patent

set(CMAKE_C_STANDARD 23)
set(CMAKE_CXX_STANDARD 20)

# Interface library carrying all example-specific flags
add_library(clltk_example_flags INTERFACE)
target_compile_options(clltk_example_flags INTERFACE -O0 -Wno-double-promotion -Wno-date-time)
target_link_libraries(clltk_example_flags INTERFACE clltk_warnings_extra)
if(CMAKE_C_COMPILER_ID MATCHES "Clang")
    target_compile_options(clltk_example_flags INTERFACE
        -Weverything
        -Wno-newline-eof
        -Wno-format-pedantic
        -Wno-global-constructors
        -Wno-exit-time-destructors
        -Wno-unneeded-internal-declaration
        -Wno-pre-c23-compat
        -Wno-c++98-compat
        -Wno-c++98-compat-pedantic
        -Wno-pre-c++17-compat
        -Wno-integer-overflow
        -Wno-missing-prototypes
        -Wno-reserved-identifier
        -Wno-old-style-cast
        -Wno-extra-semi
    )
endif()

add_subdirectory(./simple_c)
add_subdirectory(./complex_c)
add_subdirectory(./format_c)
add_subdirectory(./extreme_c)
add_subdirectory(./with_libraries)
add_subdirectory(./simple_cpp)
add_subdirectory(./complex_cpp)
add_subdirectory(./process_threads)
add_subdirectory(./snapshot_cpp)

# Custom target will always cause its dependencies to be evaluated and is
# run by default
add_custom_target(clean_up_traces ALL
    DEPENDS
    clean_up_traces_command
)

# clean_up_traces_command will always be rebuilt because it depends on always_rebuild
if(EXISTS "/tmp/traces")
    add_custom_command(
        OUTPUT clean_up_traces_command
        COMMAND find ${CMAKE_CURRENT_BINARY_DIR} "/tmp/traces" "\\(" -name "*.clltk_trace" -o -name "additional_tracepoints.json" "\\)" -delete -print
    )
else()
    add_custom_command(
        OUTPUT clean_up_traces_command
        COMMAND find ${CMAKE_CURRENT_BINARY_DIR} "\\(" -name "*.clltk_trace" -o -name "additional_tracepoints.json" "\\)" -delete -print
    )
endif()
