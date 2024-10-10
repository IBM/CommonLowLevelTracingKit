# Copyright (c) 2024, International Business Machines
# SPDX-License-Identifier: BSD-2-Clause-Patent



find_package(nlohmann_json REQUIRED)
find_package(CLI11 REQUIRED)

add_library(clltk-cmd-interface INTERFACE)

target_include_directories(clltk-cmd-interface
    INTERFACE
        ./include/
)

target_link_libraries(clltk-cmd-interface
    INTERFACE
        CLI11::CLI11
        nlohmann_json::nlohmann_json
)
