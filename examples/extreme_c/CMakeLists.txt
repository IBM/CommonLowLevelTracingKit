# Copyright (c) 2024, International Business Machines
# SPDX-License-Identifier: BSD-2-Clause-Patent

add_executable(example-extreme_c
    ${CMAKE_CURRENT_BINARY_DIR}/extreme_c.gen.c
)

target_link_libraries(example-extreme_c
PRIVATE
    clltk_tracing
    clltk_example_flags
)

add_dependencies(example-extreme_c
    clean_up_traces
)

add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/extreme_c.gen.c
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/extreme_c.py
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND python3 ${CMAKE_CURRENT_SOURCE_DIR}/extreme_c.py
)
