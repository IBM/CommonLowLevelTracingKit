# Copyright (c) 2024, International Business Machines
# SPDX-License-Identifier: BSD-2-Clause-Patent

add_executable(example-gen_format_c
    ${CMAKE_CURRENT_BINARY_DIR}/format_c.gen.c
)

target_link_libraries(example-gen_format_c
PRIVATE
    clltk_tracing
)

add_dependencies(example-gen_format_c
    clean_up_traces
)

add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/format_c.gen.c
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/gen_format_c.py
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND python3 ${CMAKE_CURRENT_SOURCE_DIR}/gen_format_c.py
)
