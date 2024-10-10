# Copyright (c) 2024, International Business Machines
# SPDX-License-Identifier: BSD-2-Clause-Patent


set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

if (${CMAKE_C_COMPILER_ID} MATCHES "GNU")
    add_compile_options(-O0)
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
    add_compile_options(-Weverything)
    add_compile_options(-Wno-integer-overflow)
    add_compile_options(-Wno-reserved-identifier)
    add_compile_options(-Wno-c++98-compat)
    add_compile_options(-Wno-c++98-compat-pedantic)
    add_compile_options(-Wno-pre-c++17-compat)
    add_compile_options(-Wno-old-style-cast)
    add_compile_options(-Wno-missing-prototypes)
    add_compile_options(-Wno-unneeded-internal-declaration)
    add_compile_options(-Wno-exit-time-destructors)
    add_compile_options(-Wno-global-constructors)
    add_compile_options(-Wno-format-pedantic)
    add_compile_options(-Wno-double-promotion)
    add_compile_options(-Wno-date-time)
    add_compile_options(-Wno-extra-semi)
    add_compile_options(-Wno-newline-eof)
endif()


add_subdirectory(./simple_c)
add_subdirectory(./complex_c)
add_subdirectory(./format_c)
add_subdirectory(./extreme_c)
add_subdirectory(./with_libraries)
add_subdirectory(./simple_cpp)
add_subdirectory(./complex_cpp)
add_subdirectory(./process_threads)
add_subdirectory(./snapshot_cpp)


# Custom target will always cause its dependencies to be evaluated and is
# run by default
add_custom_target(clean_up_traces ALL
    DEPENDS
        clean_up_traces_command
)

# clean_up_traces_command will always be rebuilt because it depends on always_rebuild
if(EXISTS "/tmp/traces")
    add_custom_command(
        OUTPUT clean_up_traces_command
        COMMAND find ${CMAKE_CURRENT_BINARY_DIR} "/tmp/traces" "\\(" -name "*.clltk_trace" -o -name "additional_tracepoints.json" "\\)" -delete -print
    )
else()
    add_custom_command(
        OUTPUT clean_up_traces_command
        COMMAND find ${CMAKE_CURRENT_BINARY_DIR} "\\(" -name "*.clltk_trace" -o -name "additional_tracepoints.json" "\\)" -delete -print
    )
endif()
