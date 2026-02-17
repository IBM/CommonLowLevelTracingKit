# Copyright (c) 2024, International Business Machines
# SPDX-License-Identifier: BSD-2-Clause-Patent

# Interface libraries for compile warning flags.
# Targets link against these instead of using directory-scoped add_compile_options().

# --- Base warnings (for all library and application targets) ---
add_library(clltk_warnings INTERFACE)
target_compile_options(clltk_warnings INTERFACE
    -Wall
    -Werror
    -Wextra
    -Wstack-protector
    -fstack-protector-all
    -Wvla
    -Wswitch
    -Wcast-align
    -Wdisabled-optimization
    -Winit-self
    -Wmissing-include-dirs
    -Wshadow
    -Wstrict-overflow=5
    -Wundef
    -Wuninitialized
    -Wunused-parameter
    -Wunused-variable
    -Wunused-function
    -Wunused-label
    -Wunused-value
    -Wpointer-arith
    -Wsign-compare
    -Warray-bounds
    -Wformat
)
if(CMAKE_C_COMPILER_ID MATCHES "GNU")
    target_compile_options(clltk_warnings INTERFACE -Wlogical-op)
endif()

# --- Extra warnings (for tests and examples, adds conversion checks) ---
add_library(clltk_warnings_extra INTERFACE)
target_link_libraries(clltk_warnings_extra INTERFACE clltk_warnings)
target_compile_options(clltk_warnings_extra INTERFACE
    -Wsign-conversion
    -Wconversion
)
