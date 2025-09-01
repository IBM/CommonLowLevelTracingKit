# Copyright (c) 2024, International Business Machines
# SPDX-License-Identifier: BSD-2-Clause-Patent

file(GLOB CPP_FILES "*.cpp")

add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/args_cpp.tests.cpp
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/args_cpp.py
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND python3 ${CMAKE_CURRENT_SOURCE_DIR}/args_cpp.py
)
list(APPEND CPP_FILES ${CMAKE_CURRENT_BINARY_DIR}/args_cpp.tests.cpp)

add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/format_cpp.tests.cpp
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/format_cpp.py
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND python3 ${CMAKE_CURRENT_SOURCE_DIR}/format_cpp.py
)
list(APPEND CPP_FILES ${CMAKE_CURRENT_BINARY_DIR}/format_cpp.tests.cpp)

add_executable(decoder_tests ${CPP_FILES})

target_link_libraries(decoder_tests
    GTest::gtest_main
    gmock
    clltk_tracing_static
    clltk_snapshot_static
    clltk_decoder_object # use the object lib as static and shared have limited exported symbols
)
target_compile_definitions(decoder_tests
    PRIVATE
    _CLLTK_INTERNAL
)
target_include_directories(decoder_tests
    PRIVATE
    .
    ../../decoder_tool/cpp/source
    ../../decoder_tool/cpp/source/low_level
    ../../decoder_tool/cpp/include
    ../../tracing_library/source
)

gtest_discover_tests(decoder_tests)
