# Copyright (c) 2024, International Business Machines
# SPDX-License-Identifier: BSD-2-Clause-Patent

# =============================================================================
# Static Analysis CMake Module
# =============================================================================
#
# This module provides integration for static analysis tools:
# - clang-tidy: Clang-based C/C++ linter
# - cppcheck: Static analysis tool for C/C++
# - include-what-you-use: Tool for analyzing #include directives
#
# Usage:
#   Set these options before including this file or via cmake command line:
#   cmake -DENABLE_CLANG_TIDY=ON ...
#
# Or use the step_static_analysis.sh script for standalone analysis.
# =============================================================================

include(CMakeDependentOption)

# =============================================================================
# Options
# =============================================================================

option(ENABLE_CLANG_TIDY "Enable clang-tidy static analysis during build" OFF)
option(ENABLE_CPPCHECK "Enable cppcheck static analysis during build" OFF)
option(ENABLE_IWYU "Enable include-what-you-use analysis during build" OFF)

# =============================================================================
# clang-tidy Integration
# =============================================================================

if(ENABLE_CLANG_TIDY)
    find_program(CLANG_TIDY_EXE 
        NAMES clang-tidy clang-tidy-18 clang-tidy-17 clang-tidy-16 clang-tidy-15
        DOC "Path to clang-tidy executable"
    )
    
    if(CLANG_TIDY_EXE)
        message(STATUS "clang-tidy found: ${CLANG_TIDY_EXE}")
        
        # Use project's .clang-tidy config file
        set(CMAKE_CXX_CLANG_TIDY "${CLANG_TIDY_EXE}")
        set(CMAKE_C_CLANG_TIDY "${CLANG_TIDY_EXE}")
        
        # Optionally add extra arguments
        # set(CMAKE_CXX_CLANG_TIDY "${CLANG_TIDY_EXE};--warnings-as-errors=*")
    else()
        message(WARNING "ENABLE_CLANG_TIDY is ON but clang-tidy was not found")
    endif()
endif()

# =============================================================================
# cppcheck Integration
# =============================================================================

if(ENABLE_CPPCHECK)
    find_program(CPPCHECK_EXE 
        NAMES cppcheck
        DOC "Path to cppcheck executable"
    )
    
    if(CPPCHECK_EXE)
        message(STATUS "cppcheck found: ${CPPCHECK_EXE}")
        
        set(CMAKE_CXX_CPPCHECK 
            "${CPPCHECK_EXE}"
            "--enable=warning,performance,portability"
            "--inline-suppr"
            "--suppress=missingIncludeSystem"
            "--suppress=unmatchedSuppression"
            "--quiet"
            "--template={file}:{line}: [{severity}] {id}: {message}"
        )
        set(CMAKE_C_CPPCHECK ${CMAKE_CXX_CPPCHECK})
    else()
        message(WARNING "ENABLE_CPPCHECK is ON but cppcheck was not found")
    endif()
endif()

# =============================================================================
# include-what-you-use Integration
# =============================================================================

if(ENABLE_IWYU)
    find_program(IWYU_EXE 
        NAMES include-what-you-use iwyu
        DOC "Path to include-what-you-use executable"
    )
    
    if(IWYU_EXE)
        message(STATUS "include-what-you-use found: ${IWYU_EXE}")
        
        # Check if mapping file exists
        if(EXISTS "${CMAKE_SOURCE_DIR}/.iwyu.imp")
            set(CMAKE_CXX_INCLUDE_WHAT_YOU_USE 
                "${IWYU_EXE}"
                "-Xiwyu" "--mapping_file=${CMAKE_SOURCE_DIR}/.iwyu.imp"
                "-Xiwyu" "--no_fwd_decls"
            )
        else()
            set(CMAKE_CXX_INCLUDE_WHAT_YOU_USE 
                "${IWYU_EXE}"
                "-Xiwyu" "--no_fwd_decls"
            )
        endif()
    else()
        message(WARNING "ENABLE_IWYU is ON but include-what-you-use was not found")
    endif()
endif()

# =============================================================================
# Custom Targets for Manual Analysis
# =============================================================================
# These targets allow running static analysis on demand:
#   cmake --build build --target clang-tidy-check
#   cmake --build build --target cppcheck-check
#   cmake --build build --target static-analysis-check

if(CMAKE_EXPORT_COMPILE_COMMANDS)
    # clang-tidy target
    add_custom_target(clang-tidy-check
        COMMAND "${CMAKE_SOURCE_DIR}/scripts/ci-cd/step_static_analysis.sh" --clang-tidy
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        COMMENT "Running clang-tidy on all source files"
        VERBATIM
    )

    # cppcheck target
    add_custom_target(cppcheck-check
        COMMAND "${CMAKE_SOURCE_DIR}/scripts/ci-cd/step_static_analysis.sh" --cppcheck
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        COMMENT "Running cppcheck on all source files"
        VERBATIM
    )

    # iwyu target
    add_custom_target(iwyu-check
        COMMAND "${CMAKE_SOURCE_DIR}/scripts/ci-cd/step_static_analysis.sh" --iwyu
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        COMMENT "Running include-what-you-use on all source files"
        VERBATIM
    )

    # Combined target
    add_custom_target(static-analysis-check
        COMMAND "${CMAKE_SOURCE_DIR}/scripts/ci-cd/step_static_analysis.sh" --all
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        COMMENT "Running all static analysis tools"
        VERBATIM
    )
endif()
