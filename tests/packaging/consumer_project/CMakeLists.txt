# Copyright (c) 2024, International Business Machines
# SPDX-License-Identifier: BSD-2-Clause-Patent

cmake_minimum_required(VERSION 3.20)
project(clltk_consumer_test LANGUAGES C)

find_package(CLLTK REQUIRED)

add_executable(consumer main.c)
target_link_libraries(consumer PRIVATE clltk::clltk_tracing_shared)
