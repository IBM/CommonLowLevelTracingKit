# Copyright (c) 2024, International Business Machines
# SPDX-License-Identifier: BSD-2-Clause-Patent

add_subdirectory(./ringbuffer.tests)
add_subdirectory(./unique_stack.tests)
add_subdirectory(./crc8.tests)
add_subdirectory(./md5.tests)
add_subdirectory(./definition.tests)
add_subdirectory(./api.tests)
add_subdirectory(./arguments.tests)
add_subdirectory(./macro.tests)
add_subdirectory(./vector.tests)
add_subdirectory(./abstraction.tests)
add_subdirectory(./meta.tests)
if(CLLTK_SNAPSHOT)
    add_subdirectory(./snapshot.tests)
endif()
