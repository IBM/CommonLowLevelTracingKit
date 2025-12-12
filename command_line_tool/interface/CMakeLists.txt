# Copyright (c) 2024, International Business Machines
# SPDX-License-Identifier: BSD-2-Clause-Patent



find_package(RapidJSON REQUIRED)
find_package(CLI11 REQUIRED)
find_package(Boost REQUIRED COMPONENTS regex)
find_package(ZLIB REQUIRED)

set(INTERFACE_SOURCES
    filter.cpp
    gzip_output.cpp
    ordered_buffer.cpp
    output.cpp
    timespec.cpp
)

add_library(clltk-cmd-interface STATIC ${INTERFACE_SOURCES})

target_include_directories(clltk-cmd-interface
    PUBLIC
        .
)

target_link_libraries(clltk-cmd-interface
    PUBLIC
        CLI11::CLI11
        Boost::regex
        clltk_decoder_static
        ZLIB::ZLIB
)

target_include_directories(clltk-cmd-interface
    PUBLIC
        ${RAPIDJSON_INCLUDE_DIRS}
)
