

include(GNUInstallDirs)

set(CPACK_PACKAGE_NAME "CommonLowLevelTracingKit")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "CommonLowLevelTracingKit - common low level tracing kit")
set(CPACK_PACKAGE_DESCRIPTION "a fast, lightweight, binary, always-on, printf-style, file-based and flight-recorder-like tracing library")
set(CPACK_PACKAGE_VERSION_MAJOR ${CLLTK_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${CLLTK_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${CLLTK_VERSION_PATCH})
set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/IBM/CommonLowLevelTracingKit")
set(CPACK_PACKAGE_VENDOR "IBM by Eduard Stefes <Eduard.Stefes@ibm.com> - Johannes Staib <Johannes.Staib@ibm.com>")
set(CPACK_PACKAGE_CHECKSUM MD5)

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
set(CPACK_RPM_snapshot_PACKAGE_REQUIRES "tar >= 0.0.0, z >= 1.3")
set(CPACK_RPM_PACKAGE_RELEASE 1)
set(CPACK_RPM_BUILD_SOURCE_DIRS_PREFIX "/clltk-debug")
set(CPACK_RPM_PACKAGE_LICENSE "BSD-2-Clause-Patent")
set(CPACK_OUTPUT_FILE_PREFIX "${CMAKE_BINARY_DIR}/packages")


include(CPack)
