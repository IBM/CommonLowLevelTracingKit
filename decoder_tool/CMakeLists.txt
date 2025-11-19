# Copyright (c) 2024, International Business Machines
# SPDX-License-Identifier: BSD-2-Clause-Patent



if(CLLTK_CPP_DECODER)
    message(STATUS "add cpp decoder")
    add_subdirectory(cpp)
endif()

if(CLLTK_PYTHON_DECODER)
    message(STATUS "add python clltk_decoder")
    include(GNUInstallDirs)
    install(
        PROGRAMS ./python/clltk_decoder.py
        TYPE BIN
        COMPONENT decoder
    )
endif()
