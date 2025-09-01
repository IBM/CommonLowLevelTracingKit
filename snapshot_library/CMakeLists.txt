# Copyright (c) 2024, International Business Machines
# SPDX-License-Identifier: BSD-2-Clause-Patent



add_library(clltk_snapshot_static STATIC
    source/file.cpp
    source/snapshot_tar.cpp
    source/snapshot_gzip.cpp
)

target_include_directories(clltk_snapshot_static
    PUBLIC
        ./include/
)

target_link_libraries(clltk_snapshot_static
    PRIVATE
        tar
        z
)


set_target_properties(clltk_snapshot_static
    PROPERTIES
        OUTPUT_NAME "clltk_snapshot"
        CXX_STANDARD 17
        CXX_EXTENSIONS OFF
        CXX_STANDARD_REQUIRED ON
        COMPILE_WARNING_AS_ERROR ON
)

add_library(clltk_snapshot_shared SHARED
    source/file.cpp
    source/snapshot_tar.cpp
    source/snapshot_gzip.cpp
)

target_include_directories(clltk_snapshot_shared
    PUBLIC
        ./include/
)

target_link_libraries(clltk_snapshot_shared
    PRIVATE
        tar
        z
)

if (${CMAKE_C_COMPILER_ID} MATCHES "GNU")
    add_compile_options(-O3)
    add_compile_options(-Wall)
    add_compile_options(-Werror)
    add_compile_options(-Wextra)
    add_compile_options(-Wstack-protector)
    add_compile_options(-fstack-protector-all)
    add_compile_options(-Wvla)
    add_compile_options(-Wswitch)
    add_compile_options(-Wlogical-op)
    add_compile_options(-Wcast-align)
    add_compile_options(-Wdisabled-optimization)
    add_compile_options(-Winit-self)
    add_compile_options(-Wmissing-include-dirs)
    add_compile_options(-Wshadow)
    add_compile_options(-Wstrict-overflow=5)
    add_compile_options(-Wundef)
    add_compile_options(-Wuninitialized)
    add_compile_options(-Wunused-parameter)
    add_compile_options(-Wunused-variable)
    add_compile_options(-Wunused-function)
    add_compile_options(-Wunused-label)
    add_compile_options(-Wunused-value)
    add_compile_options(-Wpointer-arith)
    add_compile_options(-Wsign-compare)
    add_compile_options(-Wsign-conversion)
    add_compile_options(-Wconversion)
    add_compile_options(-Warray-bounds)
    add_compile_options(-Wformat)
elseif(${CMAKE_C_COMPILER_ID} MATCHES "Clang")
    add_compile_options(-Wall)
endif()

set_target_properties(clltk_snapshot_shared
    PROPERTIES
        OUTPUT_NAME "clltk_snapshot"
        CXX_STANDARD 17
        CXX_EXTENSIONS OFF
        CXX_STANDARD_REQUIRED ON
        COMPILE_WARNING_AS_ERROR ON
)

set_target_properties(clltk_snapshot_shared
    PROPERTIES 
        VERSION "1.0.0"
        SOVERSION 1
)


set_target_properties(clltk_snapshot_static PROPERTIES
    PUBLIC_HEADER include/CommonLowLevelTracingKit/snapshot.hpp
)
install(TARGETS clltk_snapshot_static
    DESTINATION ${CMAKE_INSTALL_LIBDIR}
    COMPONENT snapshot
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/CommonLowLevelTracingKit
)

set_target_properties(clltk_snapshot_shared PROPERTIES
    PUBLIC_HEADER include/CommonLowLevelTracingKit/snapshot.hpp
)
install(TARGETS clltk_snapshot_shared
    DESTINATION ${CMAKE_INSTALL_LIBDIR}
    COMPONENT snapshot
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/CommonLowLevelTracingKit
)
