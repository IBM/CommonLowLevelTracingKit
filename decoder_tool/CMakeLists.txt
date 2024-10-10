# Copyright (c) 2024, International Business Machines
# SPDX-License-Identifier: BSD-2-Clause-Patent


include(GNUInstallDirs)
install(
    PROGRAMS ./python/clltk_decoder.py
    TYPE BIN
    COMPONENT decoder
)
