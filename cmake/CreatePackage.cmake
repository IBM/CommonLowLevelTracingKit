# Copyright (c) 2024, International Business Machines
# SPDX-License-Identifier: BSD-2-Clause-Patent

include(GNUInstallDirs)

# --- Main package metadata ---
set(CPACK_PACKAGE_NAME "clltk")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Common Low Level Tracing Kit - High-performance binary tracing library")
set(CPACK_PACKAGE_DESCRIPTION "A fast, lightweight, binary, always-on, printf-style, file-based and flight-recorder-like tracing library for C/C++ applications. Provides low-overhead tracing with binary format support, kernel-space tracing capabilities, and comprehensive debugging tools.")
set(CPACK_PACKAGE_VERSION_MAJOR ${CLLTK_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${CLLTK_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${CLLTK_VERSION_PATCH})
set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/IBM/CommonLowLevelTracingKit")
set(CPACK_PACKAGE_VENDOR "IBM")
set(CPACK_PACKAGE_CONTACT "Eduard Stefes <Eduard.Stefes@ibm.com>, Johannes Staib <Johannes.Staib@ibm.com>")
set(CPACK_PACKAGE_CHECKSUM SHA256)

# --- Component definitions ---
# Each component produces its own RPM when CPACK_RPM_COMPONENT_INSTALL=ON.
# No GROUP assignment — groups cause multiple components to merge into one RPM.

# Runtime shared libraries
set(CPACK_COMPONENT_TRACING_DISPLAY_NAME "CLLTK Tracing Runtime Library")
set(CPACK_COMPONENT_TRACING_DESCRIPTION "Core tracing shared library (libclltk_tracing.so)")

set(CPACK_COMPONENT_DECODER_LIBS_DISPLAY_NAME "CLLTK Decoder Runtime Library")
set(CPACK_COMPONENT_DECODER_LIBS_DESCRIPTION "C++ decoder shared library (libclltk_decoder.so)")

set(CPACK_COMPONENT_SNAPSHOT_DISPLAY_NAME "CLLTK Snapshot Runtime Library")
set(CPACK_COMPONENT_SNAPSHOT_DESCRIPTION "Snapshot shared library (libclltk_snapshot.so)")

# Development files (headers, cmake config, pkg-config, .so symlinks)
set(CPACK_COMPONENT_DEVEL_DISPLAY_NAME "CLLTK Development Files")
set(CPACK_COMPONENT_DEVEL_DESCRIPTION "Headers, CMake config, and pkg-config files for developing against CLLTK")

# Static libraries
set(CPACK_COMPONENT_STATIC_DISPLAY_NAME "CLLTK Static Libraries")
set(CPACK_COMPONENT_STATIC_DESCRIPTION "Static archive libraries (.a) for CLLTK")

# Command line tools
set(CPACK_COMPONENT_CMD_DISPLAY_NAME "CLLTK Command Line Tools")
set(CPACK_COMPONENT_CMD_DESCRIPTION "Command line utilities for trace management")

# Python decoder
set(CPACK_COMPONENT_DECODER_DISPLAY_NAME "CLLTK Python Decoder")
set(CPACK_COMPONENT_DECODER_DESCRIPTION "Python trace decoder script")

# --- Source package configuration ---
# Include all build-relevant files; exclude docs, tests, examples, dotfiles, build dir
set(CPACK_SOURCE_IGNORE_FILES
    "${CMAKE_BINARY_DIR}"
    "${CMAKE_SOURCE_DIR}/docs/"
    "${CMAKE_SOURCE_DIR}/tests/"
    "${CMAKE_SOURCE_DIR}/examples/"
    "${CMAKE_SOURCE_DIR}/\\\\..*"
)

# --- RPM-specific configuration ---
set(CPACK_RPM_FILE_NAME "RPM-DEFAULT")
set(CPACK_RPM_PACKAGE_RELEASE 1)
set(CPACK_RPM_PACKAGE_RELEASE_DIST ON)
set(CPACK_RPM_PACKAGE_LICENSE "BSD-2-Clause-Patent")
set(CPACK_RPM_BUILD_SOURCE_DIRS_PREFIX "/clltk-debug")

# Per-component RPM package names
set(CPACK_RPM_TRACING_PACKAGE_NAME "clltk-tracing")
set(CPACK_RPM_DECODER_LIBS_PACKAGE_NAME "clltk-decoder")
set(CPACK_RPM_SNAPSHOT_PACKAGE_NAME "clltk-snapshot")
set(CPACK_RPM_DEVEL_PACKAGE_NAME "clltk-devel")
set(CPACK_RPM_STATIC_PACKAGE_NAME "clltk-static")
set(CPACK_RPM_CMD_PACKAGE_NAME "clltk-tools")
set(CPACK_RPM_DECODER_PACKAGE_NAME "clltk-python-decoder")

# Per-component RPM package summaries
set(CPACK_RPM_TRACING_PACKAGE_SUMMARY "CLLTK tracing shared library")
set(CPACK_RPM_DECODER_LIBS_PACKAGE_SUMMARY "CLLTK C++ decoder shared library")
set(CPACK_RPM_SNAPSHOT_PACKAGE_SUMMARY "CLLTK snapshot shared library")
set(CPACK_RPM_DEVEL_PACKAGE_SUMMARY "Development files for CLLTK")
set(CPACK_RPM_STATIC_PACKAGE_SUMMARY "Static libraries for CLLTK")
set(CPACK_RPM_CMD_PACKAGE_SUMMARY "CLLTK command line tools")
set(CPACK_RPM_DECODER_PACKAGE_SUMMARY "CLLTK Python decoder script")

# Per-component RPM dependencies
set(CPACK_RPM_DEVEL_PACKAGE_REQUIRES "clltk-tracing = ${CLLTK_VERSION_STRING}")
set(CPACK_RPM_STATIC_PACKAGE_REQUIRES "clltk-devel = ${CLLTK_VERSION_STRING}")
set(CPACK_RPM_CMD_PACKAGE_REQUIRES "clltk-tracing = ${CLLTK_VERSION_STRING}")

# Python decoder architecture
set(CPACK_RPM_DECODER_PACKAGE_ARCHITECTURE "noarch")

# Build dependencies for SRPM
set(CPACK_RPM_BUILDREQUIRES "cmake >= 3.20, gcc >= 10, gcc-c++, boost-devel, libarchive-devel, libffi-devel, zlib-devel")

# ldconfig scriptlets for shared library components
set(LDCONFIG_SCRIPTLET "${CMAKE_CURRENT_SOURCE_DIR}/cmake/rpm_ldconfig.sh")
set(CPACK_RPM_tracing_POST_INSTALL_SCRIPT_FILE "${LDCONFIG_SCRIPTLET}")
set(CPACK_RPM_tracing_POST_UNINSTALL_SCRIPT_FILE "${LDCONFIG_SCRIPTLET}")
set(CPACK_RPM_decoder_libs_POST_INSTALL_SCRIPT_FILE "${LDCONFIG_SCRIPTLET}")
set(CPACK_RPM_decoder_libs_POST_UNINSTALL_SCRIPT_FILE "${LDCONFIG_SCRIPTLET}")
set(CPACK_RPM_snapshot_POST_INSTALL_SCRIPT_FILE "${LDCONFIG_SCRIPTLET}")
set(CPACK_RPM_snapshot_POST_UNINSTALL_SCRIPT_FILE "${LDCONFIG_SCRIPTLET}")

# Exclude standard directories from auto filelist
set(CPACK_RPM_EXCLUDE_FROM_AUTO_FILELIST_ADDITION
    "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}/cmake"
    "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}/pkgconfig"
)

# SRPM build params: pass the rpm preset so rebuilders get correct options
set(CPACK_RPM_SOURCE_PKG_BUILD_PARAMS "--preset rpm")

# Output location
set(CPACK_OUTPUT_FILE_PREFIX "${CMAKE_BINARY_DIR}/packages")

include(CPack)
