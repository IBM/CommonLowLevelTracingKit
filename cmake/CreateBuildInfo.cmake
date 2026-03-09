# Copyright (c) 2024, International Business Machines
# SPDX-License-Identifier: BSD-2-Clause-Patent

# ============================================================================
# Build info generation (for command line tool)
# ============================================================================
# This generates build_info.gen.h which contains:
# - Version string
# - Git commit hash and dirty flag  
# - Project metadata (maintainer, license, URL)
# - Feature flags (which components are enabled)
#
# Note: This is separate from version.gen.h because:
# - version.gen.h is used by the tracing library (including kernel module)
# - build_info.gen.h is only used by the command line tool
# - The kernel build doesn't use CMake, so it can't determine feature flags
#
# This file must be included AFTER all CLLTK_* options are defined.
# ============================================================================

# Convert CMake options to 1/0 for the shell script
# These need to be evaluated at build time, but since options don't change
# after configure, we can set them here
if(CLLTK_TRACING)
    set(CLLTK_HAS_TRACING_FLAG 1)
else()
    set(CLLTK_HAS_TRACING_FLAG 0)
endif()

if(CLLTK_SNAPSHOT)
    set(CLLTK_HAS_SNAPSHOT_FLAG 1)
else()
    set(CLLTK_HAS_SNAPSHOT_FLAG 0)
endif()

if(CLLTK_CPP_DECODER)
    set(CLLTK_HAS_CPP_DECODER_FLAG 1)
else()
    set(CLLTK_HAS_CPP_DECODER_FLAG 0)
endif()

if(CLLTK_PYTHON_DECODER)
    set(CLLTK_HAS_PYTHON_DECODER_FLAG 1)
else()
    set(CLLTK_HAS_PYTHON_DECODER_FLAG 0)
endif()

if(CLLTK_KERNEL_TRACING)
    set(CLLTK_HAS_KERNEL_TRACING_FLAG 1)
else()
    set(CLLTK_HAS_KERNEL_TRACING_FLAG 0)
endif()

add_custom_target(clltk-build-info-file
    COMMAND ${CMAKE_COMMAND} -E env
            CLLTK_HAS_TRACING=${CLLTK_HAS_TRACING_FLAG}
            CLLTK_HAS_SNAPSHOT=${CLLTK_HAS_SNAPSHOT_FLAG}
            CLLTK_HAS_CPP_DECODER=${CLLTK_HAS_CPP_DECODER_FLAG}
            CLLTK_HAS_PYTHON_DECODER=${CLLTK_HAS_PYTHON_DECODER_FLAG}
            CLLTK_HAS_KERNEL_TRACING=${CLLTK_HAS_KERNEL_TRACING_FLAG}
            ${CMAKE_CURRENT_LIST_DIR}/gen_version_header.sh
            -b ${CMAKE_CURRENT_LIST_DIR}/build_info.h.template
            -B ${CLLTK_VERSION_INCLUDE_PATH}/CommonLowLevelTracingKit/build_info.gen.h
    BYPRODUCTS 
        ${CLLTK_VERSION_INCLUDE_PATH}/CommonLowLevelTracingKit/build_info.gen.h
    DEPENDS 
        clltk-version-file
        ${CMAKE_CURRENT_LIST_DIR}/gen_version_header.sh
        ${CMAKE_CURRENT_LIST_DIR}/build_info.h.template
        ${CMAKE_CURRENT_LIST_DIR}/../VERSION.md
)

add_library(clltk-build-info INTERFACE)
target_include_directories(clltk-build-info
    INTERFACE
        $<BUILD_INTERFACE:${CLLTK_VERSION_INCLUDE_PATH}>
)

add_dependencies(clltk-build-info clltk-build-info-file)
