# Copyright (c) 2024, International Business Machines
# SPDX-License-Identifier: BSD-2-Clause-Patent

set_directory_properties(
PROPERTIES
    COMPILE_LANGUAGE CXX
    CXX_STANDARD 20
    CXX_EXTENSIONS ON
    CXX_STANDARD_REQUIRED ON
    C_STANDARD 11
    C_EXTENSIONS ON
    C_STANDARD_REQUIRED ON
    COMPILE_WARNING_AS_ERROR ON
)

if (${CMAKE_C_COMPILER_ID} MATCHES "GNU")
    add_compile_options(-O0)
    add_compile_options(-Wall)
    add_compile_options(-Werror)
    add_compile_options(-Wextra)
    add_compile_options(-Wstack-protector)
    add_compile_options(-fstack-protector-all)
    add_compile_options(-Wvla)
    add_compile_options(-Wswitch)
    add_compile_options(-Wlogical-op)
    add_compile_options(-Wcast-align)
    add_compile_options(-Wdisabled-optimization)
    add_compile_options(-Winit-self)
    add_compile_options(-Wmissing-include-dirs)
    add_compile_options(-Wshadow)
    add_compile_options(-Wstrict-overflow=5)
    add_compile_options(-Wundef)
    add_compile_options(-Wuninitialized)
    add_compile_options(-Wunused-parameter)
    add_compile_options(-Wunused-variable)
    add_compile_options(-Wunused-function)
    add_compile_options(-Wunused-label)
    add_compile_options(-Wunused-value)
    add_compile_options(-Wpointer-arith)
    add_compile_options(-Wsign-compare)
    add_compile_options(-Wsign-conversion)
    add_compile_options(-Wconversion)
    add_compile_options(-Warray-bounds)
    add_compile_options(-Wformat)
elseif(${CMAKE_C_COMPILER_ID} MATCHES "Clang")
    add_compile_options(-Wall)
    add_compile_options(-Wno-unneeded-internal-declaration)
    add_compile_options(-Wno-deprecated-declarations)
endif()

option(INSTALL_GMOCK OFF)
option(INSTALL_GTEST OFF)
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

add_definitions(-DUNITTEST)
add_compile_options(-O0)

add_subdirectory(tracing.tests)
add_subdirectory(robot.tests)
