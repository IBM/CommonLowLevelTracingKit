# Copyright (c) 2024, International Business Machines
# SPDX-License-Identifier: BSD-2-Clause-Patent

set(TARGET_NAME "clltk-cmd-live")

add_library(${TARGET_NAME} OBJECT
    live.cpp
    ordered_buffer.cpp
)

target_include_directories(${TARGET_NAME}
    PRIVATE
        .
        # Include decoder source directory for pool.hpp
        ${CMAKE_SOURCE_DIR}/decoder_tool/cpp/source
        ${CMAKE_SOURCE_DIR}/decoder_tool/cpp/source/low_level
)

target_link_libraries(${TARGET_NAME}
    PRIVATE
        clltk-cmd-interface
        clltk_decoder_static
)

set(CLLTK_COMMAND_LINE_COMMANDS "${CLLTK_COMMAND_LINE_COMMANDS};${TARGET_NAME}" PARENT_SCOPE)
