# Copyright (c) 2024, International Business Machines
# SPDX-License-Identifier: BSD-2-Clause-Patent

cmake_minimum_required(VERSION 3.20 FATAL_ERROR)

project(CommoneLowLevelTracingKit LANGUAGES C CXX)

include(cmake/Toolchain.cmake)
include(cmake/ToolchainVersionCheck.cmake)
include(cmake/CreateVersion.cmake)
include(cmake/CreatePackage.cmake)
include(cmake/CheckPICEnabled.cmake)
include(cmake/CodeCoverage.cmake)
include(cmake/ASAN.cmake)

if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    # We're in the root, define additional targets for developers.
    message(STATUS "clltk is standalone")
    option(STANDALONE_PROJECT "" "ON")
else()
    message(STATUS "clltk is imported")
    option(STANDALONE_PROJECT "" "OFF")
endif()

set(CLLTK_TRACING_LIB_TYPE "STATIC" CACHE STRING "make target clltk_tracing alias for clltk_tracing_shared with \"SHARED\" or clltk_tracing_static with \"STATIC\"") 
add_subdirectory(./tracing_library)
check_pic_enabled(clltk_tracing_static)
check_pic_enabled(clltk_tracing_shared)
check_pic_enabled(clltk_tracing)

option(CLLTK_SNAPSHOT "select snapshot lib for build and packaging" ON)
if(CLLTK_SNAPSHOT)
    message(STATUS "add snapshot_library")
    add_subdirectory(./snapshot_library)
    check_pic_enabled(clltk_snapshot_static)
    check_pic_enabled(clltk_snapshot_shared)
endif()

option(CLLTK_DECODER "select python decoder for packaging" ON)
if(CLLTK_DECODER)
    message(STATUS "add clltk_decoder")
    add_subdirectory(./decoder_tool)
endif()

option(CLLTK_COMMAND_LINE_TOOL "select clltk_cmd for packaging" ON)
if(CLLTK_COMMAND_LINE_TOOL)
    message(STATUS "add command_line_tool")
    add_subdirectory(./command_line_tool)
endif()

option(CLLTK_KERNEL_TRACING "select kernel tracing module for build" ${STANDALONE_PROJECT})
if(CLLTK_KERNEL_TRACING AND EXISTS ./kernel_tracing_library )
    message(STATUS "add kernel_tracing")
    add_subdirectory(./kernel_tracing_library EXCLUDE_FROM_ALL)
endif()

option(CLLTK_EXAMPLES "Also configure and build examples" ${STANDALONE_PROJECT})
if(CLLTK_EXAMPLES AND EXISTS ./examples )
    message(STATUS "add examples")
    add_subdirectory(./examples)
endif()

option(CLLTK_TESTS "Also configure and build tests" ${STANDALONE_PROJECT})
if(CLLTK_TESTS AND EXISTS ./tests/)
    message(STATUS "add tests")
    enable_testing()
    add_subdirectory(./tests/)
endif()
