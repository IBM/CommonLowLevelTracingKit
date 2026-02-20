

find_package(Boost REQUIRED CONFIG)

set(TARGET_SOURCES
    source/low_level/file.cpp
    source/low_level/ringbuffer.cpp
    source/low_level/tracebufferfile.cpp
    source/low_level/formatter.cpp
    source/low_level/archive.cpp
    source/low_level/meta_parser.cpp
    source/low_level/stack_section.cpp
    source/low_level/elf_reader.cpp
    source/Pool.cpp
    source/ToString.cpp
    source/Tracepoint.cpp
    source/Tracebuffer.cpp
    source/Meta.cpp
)
add_library(clltk_decoder_object OBJECT
    ${TARGET_SOURCES}
)
target_include_directories(clltk_decoder_object
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
    source
    source/low_level
)

target_compile_definitions(clltk_decoder_object
    PRIVATE
    _CLLTK_INTERNAL
)

target_link_libraries(clltk_decoder_object
    PRIVATE
    Boost::boost
    atomic
    archive
    ffi
)
target_compile_options(clltk_decoder_object PRIVATE
    -Wall
    -Werror
    -Wextra

    # Additional, useful warnings
    -Wpedantic
    -Wshadow
    -Wformat=2
    -Wcast-align
    -Wstrict-aliasing=2
    -Wcast-qual
    -Wundef
    -Wswitch-enum
    -Wunreachable-code
    -Wnull-dereference
    -Wdouble-promotion
    -Wconversion
    -Wsign-conversion
    -Wmissing-declarations
    -Wwrite-strings
    
    # Suppress warning about deprecated <ciso646> header in Boost (C++20 compatibility)
    -Wno-cpp
)
if (${CMAKE_C_COMPILER_ID} MATCHES "GNU")
    target_compile_options(clltk_decoder_object PRIVATE
        -Wduplicated-cond
        -Wduplicated-branches
        -Wlogical-op
    )
endif()
target_compile_features(clltk_decoder_object PUBLIC cxx_std_20)
set_target_properties(clltk_decoder_object PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
    INTERPROCEDURAL_OPTIMIZATION_RELEASE ON
    INTERPROCEDURAL_OPTIMIZATION_RELWITHDEBINFO ON
)
set(PUBLIC_HEADERS 
        include/CommonLowLevelTracingKit/decoder/Common.hpp
        include/CommonLowLevelTracingKit/decoder/Meta.hpp
        include/CommonLowLevelTracingKit/decoder/Tracebuffer.hpp
        include/CommonLowLevelTracingKit/decoder/Tracepoint.hpp
)

add_library(clltk_decoder_static STATIC
    $<TARGET_OBJECTS:clltk_decoder_object>
)
set_target_properties(clltk_decoder_static
    PROPERTIES
        OUTPUT_NAME "clltk_decoder_static"
)

add_library(clltk_decoder_shared SHARED
    $<TARGET_OBJECTS:clltk_decoder_object>
)
set_target_properties(clltk_decoder_shared
    PROPERTIES
        OUTPUT_NAME "clltk_decoder"
)

foreach(LIB_NAME clltk_decoder_static clltk_decoder_shared)
    target_include_directories(${LIB_NAME}
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    )
    target_link_libraries(${LIB_NAME}
        PRIVATE
            clltk_decoder_object
    )
    target_compile_features(${LIB_NAME} PUBLIC cxx_std_20)
    set_target_properties(${LIB_NAME}
        PROPERTIES
            VERSION ${CLLTK_VERSION_STRING}
            SOVERSION ${CLLTK_VERSION_MAJOR}
            LINKER_LANGUAGE CXX
            CXX_VISIBILITY_PRESET hidden
            CXX_STANDARD 20
            CXX_EXTENSIONS OFF
            CXX_STANDARD_REQUIRED ON
            COMPILE_WARNING_AS_ERROR ON
            VISIBILITY_INLINES_HIDDEN ON
    )
endforeach()

set(PUBLIC_HEADERS 
        include/CommonLowLevelTracingKit/decoder/Common.hpp
        include/CommonLowLevelTracingKit/decoder/Inline.hpp
        include/CommonLowLevelTracingKit/decoder/Meta.hpp
        include/CommonLowLevelTracingKit/decoder/Pool.hpp
        include/CommonLowLevelTracingKit/decoder/ToString.hpp
        include/CommonLowLevelTracingKit/decoder/Tracebuffer.hpp
        include/CommonLowLevelTracingKit/decoder/Tracepoint.hpp
)
set_target_properties(
    clltk_decoder_static
    clltk_decoder_shared
    PROPERTIES
        PUBLIC_HEADER "${PUBLIC_HEADERS}"
)
install(TARGETS
    clltk_decoder_shared
    EXPORT CLLTKTracingTargets
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT decoder_libs
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT decoder_libs NAMELINK_COMPONENT devel
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/CommonLowLevelTracingKit/decoder/ COMPONENT devel
)
install(TARGETS
    clltk_decoder_static
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT devel
)

# pkg-config
configure_file(
    "${CMAKE_SOURCE_DIR}/cmake/clltk_decoder.pc.in"
    "${CMAKE_BINARY_DIR}/clltk_decoder.pc"
    @ONLY
)
install(FILES "${CMAKE_BINARY_DIR}/clltk_decoder.pc"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig
    COMPONENT devel
)
