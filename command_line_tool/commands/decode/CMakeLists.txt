# Copyright (c) 2024, International Business Machines
# SPDX-License-Identifier: BSD-2-Clause-Patent

set(TARGET_NAME "clltk-cmd-decoder")

add_library(${TARGET_NAME} OBJECT
    decoder.cpp
    timespec.cpp
    filter.cpp
)

target_link_directories(${TARGET_NAME}
    PRIVATE
        .
)

target_link_libraries(${TARGET_NAME}
    PRIVATE
        clltk-cmd-interface
        clltk_decoder_static
)

set(CLLTK_COMMAND_LINE_COMMANDS "${CLLTK_COMMAND_LINE_COMMANDS};${TARGET_NAME}" PARENT_SCOPE)
