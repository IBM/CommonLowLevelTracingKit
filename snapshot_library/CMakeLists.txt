# Copyright (c) 2024, International Business Machines
# SPDX-License-Identifier: BSD-2-Clause-Patent

add_library(clltk_snapshot_static STATIC
    source/file.cpp
    source/snapshot.cpp
)
add_library(clltk_snapshot_shared SHARED
    source/file.cpp
    source/snapshot.cpp
)
foreach(LIB_NAME clltk_snapshot_static clltk_snapshot_shared)
    target_include_directories(${LIB_NAME}
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    )

	target_link_libraries(${LIB_NAME}
		PRIVATE
			archive
			clltk_warnings
	)

    target_compile_options(${LIB_NAME} PRIVATE -O3)

    set_target_properties(${LIB_NAME}
        PROPERTIES
            CXX_STANDARD 20
            CXX_EXTENSIONS OFF
            CXX_STANDARD_REQUIRED ON
            COMPILE_WARNING_AS_ERROR ON
            VERSION ${CLLTK_VERSION_STRING}
            SOVERSION ${CLLTK_VERSION_MAJOR}
            LINKER_LANGUAGE CXX
            CXX_VISIBILITY_PRESET hidden
    )
endforeach()

set_target_properties(clltk_snapshot_static
    PROPERTIES
        OUTPUT_NAME "clltk_snapshot_static"
)

set_target_properties(clltk_snapshot_shared
    PROPERTIES
        OUTPUT_NAME "clltk_snapshot"
)

set_target_properties(
    clltk_snapshot_static
    clltk_snapshot_shared
    PROPERTIES
        PUBLIC_HEADER include/CommonLowLevelTracingKit/snapshot/snapshot.hpp
)
install(TARGETS
    clltk_snapshot_shared
    EXPORT CLLTKTracingTargets
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT snapshot
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT snapshot NAMELINK_COMPONENT devel
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/CommonLowLevelTracingKit/snapshot/ COMPONENT devel
)
install(TARGETS
    clltk_snapshot_static
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT devel
)

# pkg-config (only when built from the main project tree where the .pc.in template exists)
set(_CLLTK_SNAPSHOT_PC_IN "${CMAKE_CURRENT_SOURCE_DIR}/../cmake/clltk_snapshot.pc.in")
if(EXISTS "${_CLLTK_SNAPSHOT_PC_IN}")
    configure_file(
        "${_CLLTK_SNAPSHOT_PC_IN}"
        "${CMAKE_BINARY_DIR}/clltk_snapshot.pc"
        @ONLY
    )
    install(FILES "${CMAKE_BINARY_DIR}/clltk_snapshot.pc"
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig
        COMPONENT devel
    )
endif()
