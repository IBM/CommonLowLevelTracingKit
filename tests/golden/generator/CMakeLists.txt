# Copyright (c) 2026, International Business Machines
# SPDX-License-Identifier: BSD-2-Clause-Patent

# Golden fixture generator.
#
# The writer links frozen_info.c ahead of the tracing static archive: its
# definitions of info_get_timestamp_ns/process_id/thread_id satisfy the
# references so the archive's info.o is never pulled, freezing timestamp, pid,
# tid, and (through the span-id salt derived from them) span ids. Generation is
# therefore byte-deterministic, which is what lets the golden gate decide "did
# the format change?" with a plain byte compare. The core library is unchanged.
#
# One C++ writer exercises every tracepoint kind (printf, dump, dynamic, spans,
# fmt) into a single GOLDEN buffer -- fmt is C++20-only, the rest compile
# identically from C, so one writer covers the whole decoder surface.
#
# Build and run with the `golden` preset:
#   cmake --preset golden && cmake --build --preset golden --target golden-generate
# the fixture lands in <build>/golden/.

set(CMAKE_CXX_STANDARD 20)

set(GOLDEN_OUTPUT_DIR "${CMAKE_BINARY_DIR}/golden")

add_executable(golden_writer writer.cpp frozen_info.c)
target_link_libraries(golden_writer PRIVATE clltk_tracing_static)

# ELF metadata golden objects (committed for s390x only): the same writer.cpp
# compiled as a relocatable object (elfm.o, relocation-record path) and a shared
# object (elfm.so, virtual-address path) that `clltk meta` must parse. Not run,
# so no frozen identity and no debug info; only the tracepoint metadata sections
# matter. The objects leave the clltk runtime symbols undefined, like any
# consumer's object looks to `clltk meta`.
set(_golden_elf_includes $<TARGET_PROPERTY:clltk_tracing_static,INTERFACE_INCLUDE_DIRECTORIES>)

add_library(golden_elf_obj OBJECT writer.cpp)
target_include_directories(golden_elf_obj PRIVATE ${_golden_elf_includes})
target_compile_options(golden_elf_obj PRIVATE -g0)

add_library(golden_elf_shared SHARED writer.cpp)
target_include_directories(golden_elf_shared PRIVATE ${_golden_elf_includes})
target_compile_options(golden_elf_shared PRIVATE -g0)
set_target_properties(golden_elf_shared PROPERTIES LINK_FLAGS "-Wl,--unresolved-symbols=ignore-all")

# Trace fixture: run the writer into a clean output dir. CLLTK_TRACING_PATH
# selects where the tracebuffer is written; the writer names it GOLDEN. This is
# what the format gate builds -- kept independent of the ELF objects below so an
# ELF build issue can never masquerade as a trace-format change.
add_custom_target(golden-generate
    COMMAND ${CMAKE_COMMAND} -E rm -rf "${GOLDEN_OUTPUT_DIR}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${GOLDEN_OUTPUT_DIR}"
    COMMAND ${CMAKE_COMMAND} -E env "CLLTK_TRACING_PATH=${GOLDEN_OUTPUT_DIR}"
        $<TARGET_FILE:golden_writer>
    DEPENDS golden_writer
    COMMENT "Generating golden trace fixture into ${GOLDEN_OUTPUT_DIR}"
    VERBATIM
)

# ELF metadata objects: separate target (rarely refreshed) so the format gate
# does not build them. Collects elfm.o/elfm.so into the same output dir.
add_custom_target(golden-elf
    COMMAND ${CMAKE_COMMAND} -E make_directory "${GOLDEN_OUTPUT_DIR}"
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_OBJECTS:golden_elf_obj> "${GOLDEN_OUTPUT_DIR}/elfm.o"
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:golden_elf_shared> "${GOLDEN_OUTPUT_DIR}/elfm.so"
    DEPENDS golden_elf_obj golden_elf_shared
    COMMENT "Generating golden ELF metadata objects into ${GOLDEN_OUTPUT_DIR}"
    VERBATIM
)
