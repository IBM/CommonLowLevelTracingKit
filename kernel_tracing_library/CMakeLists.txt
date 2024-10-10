# Copyright (c) 2024, International Business Machines
# SPDX-License-Identifier: BSD-2-Clause-Patent


file(GLOB_RECURSE clltk_kernel_tracing_sources *.c *.h)
file(GLOB_RECURSE clltk_kernel_tracing_output *.cmd *.mod *.o *.mod.c *.ko *.symvers *.order)
add_custom_target(clltk_kernel_tracing
    COMMAND make
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/src/
    SOURCES ${clltk_kernel_tracing_sources}
    BYPRODUCTS ${clltk_kernel_tracing_output}
)

# install is missing