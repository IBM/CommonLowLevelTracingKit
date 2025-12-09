# Copyright (c) 2024, International Business Machines
# SPDX-License-Identifier: BSD-2-Clause-Patent



find_package(RapidJSON REQUIRED)
find_package(CLI11 REQUIRED)
find_package(Boost REQUIRED COMPONENTS regex)

set(INTERFACE_SOURCES
    commands/filter.cpp
)

set(INTERFACE_HEADERS
    commands/filter.hpp
    commands/interface.hpp
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
)

target_include_directories(clltk-cmd-interface
    PUBLIC
        ${RAPIDJSON_INCLUDE_DIRS}
)
