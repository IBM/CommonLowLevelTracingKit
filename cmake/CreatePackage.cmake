

include(GNUInstallDirs)

# Main package configuration
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

# Component descriptions
set(CPACK_COMPONENT_TRACING_DISPLAY_NAME "CLLTK Runtime Libraries")
set(CPACK_COMPONENT_TRACING_DESCRIPTION "Core tracing runtime libraries (shared and static)")
set(CPACK_COMPONENT_TRACING_GROUP "Runtime")

set(CPACK_COMPONENT_CMD_DISPLAY_NAME "CLLTK Command Line Tools")
set(CPACK_COMPONENT_CMD_DESCRIPTION "Command line utilities for trace management")
set(CPACK_COMPONENT_CMD_GROUP "Tools")

set(CPACK_COMPONENT_SNAPSHOT_DISPLAY_NAME "CLLTK Snapshot Library")
set(CPACK_COMPONENT_SNAPSHOT_DESCRIPTION "Libraries for snapshot and archival functionality")
set(CPACK_COMPONENT_SNAPSHOT_GROUP "Runtime")

set(CPACK_COMPONENT_DECODER_DISPLAY_NAME "CLLTK Decoder Libraries")
set(CPACK_COMPONENT_DECODER_DESCRIPTION "Trace decoding and analysis libraries")
set(CPACK_COMPONENT_DECODER_GROUP "Runtime")

set(CPACK_SOURCE_IGNORE_FILES 
    "${CMAKE_BINARY_DIR}"
    "${CMAKE_BINARY_DIR}/CMakeFiles/.*"
    "${CMAKE_SOURCE_DIR}/docs/"
    "${CMAKE_SOURCE_DIR}/tests/"
    "${CMAKE_SOURCE_DIR}/examples/"
    "${CMAKE_SOURCE_DIR}/scripts/"
    "${CMAKE_SOURCE_DIR}/\\\\..*"
)

set(CPACK_RPM_FILE_NAME "RPM-DEFAULT")
set(CPACK_RPM_PACKAGE_RELEASE 1)
set(CPACK_RPM_BUILD_SOURCE_DIRS_PREFIX "/clltk-debug")
set(CPACK_RPM_PACKAGE_LICENSE "BSD-2-Clause-Patent")
set(CPACK_OUTPUT_FILE_PREFIX "${CMAKE_BINARY_DIR}/packages")

include(CPack)
