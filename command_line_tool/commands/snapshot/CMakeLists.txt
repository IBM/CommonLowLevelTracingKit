# Copyright (c) 2024, International Business Machines
# SPDX-License-Identifier: BSD-2-Clause-Patent


if(NOT TARGET clltk_snapshot_static)
    # command only works with snapshot library
    return()
endif()

set(TARGET_NAME "clltk-cmd-snapshot")

add_library(${TARGET_NAME} OBJECT
    snapshot.cpp
)

target_link_directories(${TARGET_NAME}
    PRIVATE
        .
)

target_link_libraries(${TARGET_NAME}
    PRIVATE
        clltk-cmd-interface
        clltk_snapshot_static
)

set(CLLTK_COMMAND_LINE_COMMANDS "${CLLTK_COMMAND_LINE_COMMANDS};${TARGET_NAME}" PARENT_SCOPE)
