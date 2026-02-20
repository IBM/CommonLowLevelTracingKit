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
set(CPACK_RPM_DEVEL_PACKAGE_REQUIRES "clltk-tracing = ${CLLTK_VERSION_STRING}, clltk-decoder = ${CLLTK_VERSION_STRING}, clltk-snapshot = ${CLLTK_VERSION_STRING}")
set(CPACK_RPM_STATIC_PACKAGE_REQUIRES "clltk-devel = ${CLLTK_VERSION_STRING}")

# Python decoder architecture
set(CPACK_RPM_DECODER_PACKAGE_ARCHITECTURE "noarch")

# ldconfig scriptlets for shared library components
# NOTE: CPack requires UPPERCASE component names in variable names
set(LDCONFIG_SCRIPTLET "${CMAKE_CURRENT_SOURCE_DIR}/cmake/rpm_ldconfig.sh")
set(CPACK_RPM_TRACING_POST_INSTALL_SCRIPT_FILE "${LDCONFIG_SCRIPTLET}")
set(CPACK_RPM_TRACING_POST_UNINSTALL_SCRIPT_FILE "${LDCONFIG_SCRIPTLET}")
set(CPACK_RPM_DECODER_LIBS_POST_INSTALL_SCRIPT_FILE "${LDCONFIG_SCRIPTLET}")
set(CPACK_RPM_DECODER_LIBS_POST_UNINSTALL_SCRIPT_FILE "${LDCONFIG_SCRIPTLET}")
set(CPACK_RPM_SNAPSHOT_POST_INSTALL_SCRIPT_FILE "${LDCONFIG_SCRIPTLET}")
set(CPACK_RPM_SNAPSHOT_POST_UNINSTALL_SCRIPT_FILE "${LDCONFIG_SCRIPTLET}")

# Exclude standard directories from auto filelist
# Use hardcoded /usr paths — CMAKE_INSTALL_PREFIX defaults to /usr/local
# but RPMs always install to /usr via CPACK_PACKAGING_INSTALL_PREFIX
set(CPACK_RPM_EXCLUDE_FROM_AUTO_FILELIST_ADDITION
    "/usr/${CMAKE_INSTALL_LIBDIR}/cmake"
    "/usr/${CMAKE_INSTALL_LIBDIR}/pkgconfig"
)

# Output location
set(CPACK_OUTPUT_FILE_PREFIX "${CMAKE_BINARY_DIR}/packages")

include(CPack)

# --- SRPM from real source tree ---
# CPack's CPACK_SOURCE_GENERATOR=RPM packages the install tree, not actual source.
# We build a proper SRPM using git archive + rpmbuild -bs with our spec file.
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/clltk.spec.in"
    "${CMAKE_BINARY_DIR}/clltk.spec"
    @ONLY
)

add_custom_target(srpm
    COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/cmake/build_srpm.sh"
        "${CMAKE_SOURCE_DIR}"
        "${CMAKE_BINARY_DIR}"
        "${CLLTK_VERSION_STRING}"
    COMMENT "Building SRPM from source tree"
    VERBATIM
)
