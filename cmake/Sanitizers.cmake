# Copyright (c) 2024, International Business Machines
# SPDX-License-Identifier: BSD-2-Clause-Patent
#
# Runtime sanitizer support. These matter more than static lint for this
# library: it packs bytes, swaps endianness, uses 48-bit bitfield offsets,
# and coordinates through shared-memory mutexes and atomics - exactly the
# semantics static analysis of a single translation unit cannot see.
#
#   ENABLE_ASAN   AddressSanitizer     use-after-free, heap/stack overflow, leaks
#   ENABLE_UBSAN  UndefinedBehavior    signed overflow, bad shifts, misalignment
#   ENABLE_TSAN   ThreadSanitizer      data races
#
# ASan and TSan cannot be combined (both hook memory); UBSan composes with
# either. Sanitizers disable LTO here - instrumentation wants real frames.

option(ENABLE_ASAN "Enable AddressSanitizer" OFF)
option(ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer" OFF)
option(ENABLE_TSAN "Enable ThreadSanitizer" OFF)

if(ENABLE_ASAN AND ENABLE_TSAN)
    message(FATAL_ERROR "ENABLE_ASAN and ENABLE_TSAN are mutually exclusive")
endif()

set(_clltk_sanitizers "")
if(ENABLE_ASAN)
    list(APPEND _clltk_sanitizers address)
endif()
if(ENABLE_UBSAN)
    list(APPEND _clltk_sanitizers undefined)
endif()
if(ENABLE_TSAN)
    list(APPEND _clltk_sanitizers thread)
endif()

if(_clltk_sanitizers)
    string(REPLACE ";" "," _clltk_sanitizer_flag "${_clltk_sanitizers}")
    message(STATUS "Sanitizers enabled: ${_clltk_sanitizer_flag}")
    add_compile_options(-fsanitize=${_clltk_sanitizer_flag} -fno-omit-frame-pointer -g)
    add_link_options(-fsanitize=${_clltk_sanitizer_flag})
    # UBSan defaults to warn-and-continue; make findings fatal so CI can gate.
    if(ENABLE_UBSAN)
        add_compile_options(-fno-sanitize-recover=undefined)
    endif()
endif()
