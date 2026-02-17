# Copyright (c) 2024, International Business Machines
# SPDX-License-Identifier: BSD-2-Clause-Patent

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_EXTENSIONS ON)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_EXTENSIONS ON)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Interface library carrying all test-specific flags
add_library(clltk_test_flags INTERFACE)
target_compile_definitions(clltk_test_flags INTERFACE UNITTEST)
target_compile_options(clltk_test_flags INTERFACE -O0 -g)
target_link_libraries(clltk_test_flags INTERFACE clltk_warnings_extra)
if(ENABLE_ASAN)
    target_compile_definitions(clltk_test_flags INTERFACE CLLTK_ASAN_ENABLED)
endif()
if(CMAKE_C_COMPILER_ID MATCHES "Clang")
    target_compile_options(clltk_test_flags INTERFACE
        -Wno-unneeded-internal-declaration
        -Wno-deprecated-declarations
    )
endif()

option(INSTALL_GMOCK "Install GMock" OFF)
option(INSTALL_GTEST "Install GTest" OFF)
include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG release-1.12.1
)
FetchContent_MakeAvailable(googletest)

include(GoogleTest)

target_compile_options(gtest PRIVATE -w)        # disable all errors and warnings
target_compile_options(gtest_main PRIVATE -w)   # disable all errors and warnings
target_compile_options(gmock PRIVATE -w)        # disable all errors and warnings
target_compile_options(gmock_main PRIVATE -w)   # disable all errors and warnings

add_subdirectory(tracing.tests)
add_subdirectory(decoder.tests)
add_subdirectory(cmd.tests)
