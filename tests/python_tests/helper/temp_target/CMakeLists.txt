# Copyright (c) 2024, International Business Machines
# SPDX-License-Identifier: BSD-2-Clause-Patent


cmake_minimum_required(VERSION 3.18)
project(python_build_test LANGUAGES C CXX)

if(NOT CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    message( FATAL_ERROR "never include this folder, it is only for python unittest")
endif()

include(../../../../cmake/CreateVersion.cmake)

set(CLLTK_TRACING_LIB_TYPE "STATIC")
add_subdirectory("../../../../tracing_library" "./tracing_library/")
add_subdirectory("../../../../snapshot_library" "./snapshot_library/")

set(main_c_file ${CMAKE_CURRENT_BINARY_DIR}/main.gen.c)
if(NOT EXISTS ${main_c_file})
    file(WRITE ${main_c_file} "#error \"empty file\"")
endif()

add_executable(main_c
    ${main_c_file} # needs to be dynamicly created by python unittest
)

target_link_libraries(main_c
    clltk_tracing_shared
)


set(main_cpp_file ${CMAKE_CURRENT_BINARY_DIR}/main.gen.cpp)
if(NOT EXISTS ${main_cpp_file})
    file(WRITE ${main_cpp_file} "#error \"empty file\"")
endif()

add_executable(main_cpp
    ${main_cpp_file} # needs to be dynamicly created by python unittest
)

target_link_libraries(main_cpp
    clltk_tracing_shared
    clltk_snapshot_static
)

set_target_properties(main_cpp
    PROPERTIES
        CXX_STANDARD 17
        CXX_EXTENSIONS OFF
        CXX_STANDARD_REQUIRED ON
        COMPILE_WARNING_AS_ERROR ON
)
