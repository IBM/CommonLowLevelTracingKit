# Copyright (c) 2024, International Business Machines
# SPDX-License-Identifier: BSD-2-Clause-Patent

# Unit tests for CLI-independent command line tool components
# These test the reusable logic without CLI11 dependencies

find_package(Boost REQUIRED CONFIG)

file(GLOB CPP_FILES "*.cpp")

add_executable(cmd_tests
    ${CPP_FILES}
)

target_link_libraries(cmd_tests
    GTest::gtest_main
    gmock
    clltk_decoder_object
    clltk-cmd-interface
    ${Boost_LIBRARIES}
)

target_compile_definitions(cmd_tests
    PRIVATE
    _CLLTK_INTERNAL
)

target_include_directories(cmd_tests
    PRIVATE
    .
    ../../command_line_tool/commands/decode
    ../../command_line_tool/commands/live
    ../../decoder_tool/cpp/source
    ../../decoder_tool/cpp/source/low_level
    ../../decoder_tool/cpp/include
)

gtest_discover_tests(cmd_tests)
