# Copyright (c) 2024, International Business Machines
# SPDX-License-Identifier: BSD-2-Clause-Patent


if(NOT TARGET clltk_tracing_static)
    # command only works with tracing library
    return()
endif()

set(TARGET_NAME "clltk-cmd-trace")

add_library(${TARGET_NAME} OBJECT
    tracebuffer.cpp
    tracepoint.cpp
    tracepipe.cpp
)

target_link_directories(${TARGET_NAME}
    PRIVATE
        .
)

target_link_libraries(${TARGET_NAME}
    PRIVATE
        clltk-cmd-interface
        clltk_tracing_static
)

set(CLLTK_COMMAND_LINE_COMMANDS "${CLLTK_COMMAND_LINE_COMMANDS};${TARGET_NAME}" PARENT_SCOPE)
