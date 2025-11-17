# Copyright (c) 2024, International Business Machines
# SPDX-License-Identifier: BSD-2-Clause-Patent


set(Exe_Name "")
get_filename_component(Exe_Name ${CMAKE_CURRENT_LIST_DIR} NAME)
string(REPLACE " " "_" Exe_Name ${Exe_Name})

file(GLOB_RECURSE Exe_Name_Sources
    "*.c" "*.cpp"
)

add_executable(${Exe_Name}
    ${Exe_Name_Sources}
)

set_property(TARGET ${Exe_Name}
    PROPERTY
    CXX_STANDARD 20
)

target_include_directories(${Exe_Name}
    PRIVATE
    ../../../tracing_library/source/
)

target_link_libraries(${Exe_Name}
    GTest::gtest_main
    GTest::gmock_main
    clltk_tracing_abstraction
)

gtest_discover_tests(${Exe_Name})

target_compile_options(${Exe_Name} PRIVATE -O0)
