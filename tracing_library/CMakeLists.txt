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

    source/definition/definition.c
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
    $<TARGET_OBJECTS:clltk_tracing_abstraction>
)
add_library(clltk_tracing_shared SHARED
    ${CLLTK_TRACING_SOURCE}
    $<TARGET_OBJECTS:clltk_tracing_abstraction>
)

foreach(LIB_NAME clltk_tracing_static clltk_tracing_shared)
    target_include_directories(${LIB_NAME}
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
        PRIVATE
            source
    )
    target_link_libraries(${LIB_NAME}
        PRIVATE
            clltk_tracing_abstraction
            clltk-version
    )
    set_target_properties(${LIB_NAME}
        PROPERTIES
            VERSION ${CLLTK_VERSION_STRING}
            SOVERSION ${CLLTK_VERSION_MAJOR}
            LINKER_LANGUAGE C
            COMPILE_LANGUAGE C
            C_STANDARD 11
            C_EXTENSIONS ON
            C_STANDARD_REQUIRED OFF
            COMPILE_WARNING_AS_ERROR ON
    )
    target_compile_options(${LIB_NAME}
        PUBLIC
            ${clltk_public_compiler_flags}
        PRIVATE
            ${clltk_private_compiler_flags}
    )

    target_link_options(${LIB_NAME}
        PRIVATE
            -Wl,--gc-sections
    )
endforeach()

set_target_properties(clltk_tracing_static
    PROPERTIES
        OUTPUT_NAME "clltk_tracing_static"
)

set_target_properties(clltk_tracing_shared
    PROPERTIES
        OUTPUT_NAME "clltk_tracing"
)

set(PUBLIC_HEADERS 
    include/CommonLowLevelTracingKit/tracing/_internal.h
    include/CommonLowLevelTracingKit/tracing/_macros.h
    include/CommonLowLevelTracingKit/tracing/_arguments.h
    include/CommonLowLevelTracingKit/tracing/_meta.h
    include/CommonLowLevelTracingKit/tracing/_user_tracing.h
    include/CommonLowLevelTracingKit/tracing/tracing.h
)

set_target_properties(
    clltk_tracing_static
    clltk_tracing_shared
    PROPERTIES 
        PUBLIC_HEADER "${PUBLIC_HEADERS}"
)
install(TARGETS
    clltk_tracing_shared
    EXPORT CLLTKTracingTargets
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT tracing
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT tracing NAMELINK_COMPONENT devel
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/CommonLowLevelTracingKit/tracing/ COMPONENT devel
)
install(TARGETS
    clltk_tracing_static
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT static
)

# pkg-config
configure_file(
    "${CMAKE_SOURCE_DIR}/cmake/clltk_tracing.pc.in"
    "${CMAKE_BINARY_DIR}/clltk_tracing.pc"
    @ONLY
)
install(FILES "${CMAKE_BINARY_DIR}/clltk_tracing.pc"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig
    COMPONENT devel
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
