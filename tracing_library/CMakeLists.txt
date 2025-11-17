# Copyright (c) 2024, International Business Machines
# SPDX-License-Identifier: BSD-2-Clause-Patent
# Common flags shared by all compilers
set(clltk_private_compiler_flags
    -fPIC
    -D_GNU_SOURCE
    -D_CLLTK_INTERNAL
    -D_CLLTK_VERSION_BY_CMAKE=${CLLTK_VERSION_UINT64}
    -Wall
    -Werror
    -Wextra
    -Wstack-protector
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
    -Wconversion
    -Warray-bounds
    -fvisibility=hidden
)
set(clltk_public_compiler_flags
    -Werror=format
    -Wformat=2
    -Wformat-security
)

if (${CMAKE_C_COMPILER_ID} MATCHES "GNU")
    list(APPEND clltk_private_compiler_flags)
    list(APPEND clltk_public_compiler_flags)

elseif(${CMAKE_C_COMPILER_ID} MATCHES "Clang")
    list(APPEND clltk_private_compiler_flags
        -Wno-format-nonliteral
    )
    list(APPEND clltk_public_compiler_flags
        -Wno-gnu-empty-initializer
        -Wno-zero-length-array
        -Wno-declaration-after-statement
        -Wno-gnu-zero-variadic-macro-arguments
        -Wno-date-time
        -Wno-pre-c11-compat
    )

else()
    message(FATAL_ERROR "No flags configured for compiler '${CMAKE_C_COMPILER_ID}'")
endif()

set(CLLTK_TRACING_SOURCE
    source/c-vector/vec.c
    source/md5/md5.c

    source/ringbuffer/ringbuffer.c
    source/ringbuffer/ringbuffer_layout_assert.c
    source/unique_stack/unique_stack.c
    source/unique_stack/unique_stack_layout_assert.c

    source/tracebuffer.c
    source/tracepoint.c
    source/tracepoint_layout_assert.c
    source/arguments.c
)

add_subdirectory(source/abstraction)


add_library(clltk_tracing_static STATIC
    ${CLLTK_TRACING_SOURCE}
)

target_include_directories(clltk_tracing_static
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        source
)

target_link_libraries(clltk_tracing_static
    PRIVATE
        clltk_tracing_abstraction
        clltk-version
)

set_target_properties(clltk_tracing_static
    PROPERTIES
        OUTPUT_NAME "clltk_tracing"
        VERSION ${CLLTK_VERSION_STRING}
        SOVERSION ${CLLTK_VERSION_MAJOR}
        COMPILE_LANGUAGE C
        C_STANDARD 11
        C_EXTENSIONS ON
        C_STANDARD_REQUIRED OFF
        COMPILE_WARNING_AS_ERROR ON
)

target_compile_options(clltk_tracing_static
    PUBLIC
        ${clltk_public_compiler_flags}
    PRIVATE
        ${clltk_private_compiler_flags}
)

add_library(clltk_tracing_shared SHARED
    ${CLLTK_TRACING_SOURCE}
)
target_include_directories(clltk_tracing_shared
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        source
)

target_link_libraries(clltk_tracing_shared
    PRIVATE
        clltk_tracing_abstraction
        clltk-version
)

set_target_properties(clltk_tracing_shared
    PROPERTIES
        OUTPUT_NAME "clltk_tracing"
        VERSION ${CLLTK_VERSION_STRING}
        SOVERSION ${CLLTK_VERSION_MAJOR}
        LINKER_LANGUAGE C
        COMPILE_LANGUAGE C
        C_STANDARD 11
        C_EXTENSIONS ON
        C_STANDARD_REQUIRED OFF
        COMPILE_WARNING_AS_ERROR ON
)

target_compile_options(clltk_tracing_shared
    PUBLIC
        ${clltk_public_compiler_flags}
    PRIVATE
        ${clltk_private_compiler_flags}
)

target_link_options(clltk_tracing_shared
    PRIVATE
        -Wl,--gc-sections
)

# create alias target clltk_tracing based on set cache option
string(TOUPPER "${CLLTK_TRACING_LIB_TYPE}" CLLTK_TRACING_LIB_TYPE)
if(${CLLTK_TRACING_LIB_TYPE} STREQUAL "STATIC")
    add_library(clltk_tracing ALIAS clltk_tracing_static)
elseif(${CLLTK_TRACING_LIB_TYPE} STREQUAL "SHARED")
    add_library(clltk_tracing ALIAS clltk_tracing_shared)
else()
    message(FATAL_ERROR "invalid value \"${CLLTK_TRACING_LIB_TYPE}\" for \"CLLTK_TRACING_LIB_TYPE\", not \"STATIC\" or \"SHARED\"")
endif()

set(PULIC_HEADERS 
    include/CommonLowLevelTracingKit/_internal.h
    include/CommonLowLevelTracingKit/_macros.h
    include/CommonLowLevelTracingKit/_arguments.h
    include/CommonLowLevelTracingKit/_meta.h
    include/CommonLowLevelTracingKit/_user_tracing.h
    include/CommonLowLevelTracingKit/tracing.h
)

set_target_properties(clltk_tracing_static PROPERTIES 
    PUBLIC_HEADER "${PULIC_HEADERS}"
)
install(TARGETS clltk_tracing_static
    DESTINATION ${CMAKE_INSTALL_LIBDIR}
    COMPONENT tracing
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/CommonLowLevelTracingKit
)

set_target_properties(clltk_tracing_shared
    PROPERTIES PUBLIC_HEADER "${PULIC_HEADERS}"
)
install(TARGETS clltk_tracing_shared
    DESTINATION ${CMAKE_INSTALL_LIBDIR}
    COMPONENT tracing
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/CommonLowLevelTracingKit
)
