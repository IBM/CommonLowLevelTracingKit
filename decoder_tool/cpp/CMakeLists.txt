

find_package(Boost REQUIRED CONFIG)

set(TARGET_SOURCES
    source/low_level/file.cpp
    source/low_level/ringbuffer.cpp
    source/low_level/tracebufferfile.cpp
    source/low_level/formatter.cpp
    source/low_level/archive.cpp
    source/Tracepoint.cpp
    source/Tracebuffer.cpp
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
    ${Boost_INCLUDE_DIRS}
)

target_compile_definitions(clltk_decoder_object
    PRIVATE
    _CLLTK_INTERNAL
)

target_link_libraries(clltk_decoder_object
    ${Boost_LIBRARIES}
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
)

add_library(clltk_decoder_static STATIC
    $<TARGET_OBJECTS:clltk_decoder_object>
)
target_include_directories(clltk_decoder_static
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(clltk_decoder_static
    PUBLIC
    clltk_decoder_object
)
target_compile_features(clltk_decoder_static PUBLIC cxx_std_20)
set_target_properties(clltk_decoder_static PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
)

add_library(clltk_decoder_shared SHARED
    $<TARGET_OBJECTS:clltk_decoder_object>
)
target_include_directories(clltk_decoder_shared
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(clltk_decoder_shared
    PUBLIC
    clltk_decoder_object
)
target_compile_features(clltk_decoder_shared PUBLIC cxx_std_20)
set_target_properties(clltk_decoder_shared PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
)