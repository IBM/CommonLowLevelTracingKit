# Copyright (c) 2024, International Business Machines
# SPDX-License-Identifier: BSD-2-Clause-Patent

cmake_minimum_required(VERSION 3.20)
project(clltk_consumer_shared_test LANGUAGES C)

find_package(CLLTK REQUIRED)

# A consumer SHARED library that expands the tracepoint macros *inside the .so*.
# This is the shape - a plugin/driver shared object that traces - whose _metaptr
# metadata section must be writable; if it regresses to read-only, its absolute
# pointers become text relocations (DT_TEXTREL). The test harness inspects this
# .so with readelf and fails on any text relocation, the same check a hardened
# packaging QA performs. Built as a plain shared library with no special link
# flags on purpose: it must be clean the way an ordinary consumer builds it,
# not only when forced with -z text.
add_library(consumer_plugin SHARED plugin.c)
target_link_libraries(consumer_plugin PRIVATE clltk::clltk_tracing_shared)

# An application that links the plugin and drives it, proving cross-.so tracing
# works end to end against the installed package.
add_executable(app app.c)
target_link_libraries(app PRIVATE consumer_plugin)
