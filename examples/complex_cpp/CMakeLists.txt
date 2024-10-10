# Copyright (c) 2024, International Business Machines
# SPDX-License-Identifier: BSD-2-Clause-Patent


add_executable(example-complex_cpp
    complex_cpp.cpp
)

target_link_libraries(example-complex_cpp
PRIVATE
    clltk_tracing
)

add_dependencies(example-complex_cpp
    clean_up_traces
)

set_target_properties(example-complex_cpp
    PROPERTIES
        CXX_STANDARD 17
        CXX_EXTENSIONS OFF
        CXX_STANDARD_REQUIRED ON
        COMPILE_WARNING_AS_ERROR ON
)

target_compile_options(example-complex_cpp
    PRIVATE
        -Og
        -fPIC
        -Wall
        -Werror
        -Wno-error=deprecated-declarations
        -Wno-error=sign-compare
        -Wno-error=c++11-compat
        -Wno-error=implicit-fallthrough
        -Wno-error=int-in-bool-context
        -Wextra
        -Werror=attributes
        -Wformat
        -Wuninitialized
        -Wunused-parameter
        -Wstack-protector
        -fstack-protector-all
)