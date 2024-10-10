#!/usr/bin/env bash
# Copyright (c) 2024, International Business Machines
# SPDX-License-Identifier: BSD-2-Clause-Patent

# use --target <target name>[<target name> ...] or --t <target name> [<target name> ...]
# to build a defined set of targets

echo
echo \< following output from bash file: $0 \>
echo

ROOT_PATH=$(git rev-parse --show-toplevel)
echo "ROOT_PATH=${ROOT_PATH}"
cd ${ROOT_PATH}

BUILD_DIR=${BUILD_DIR:-./build/}
BUILD_DIR=$(realpath $BUILD_DIR)/
echo BUILD_DIR="$BUILD_DIR"

if [ -n "${CLLTK_TRACING_PATH}" ]; then
    mkdir -p ${CLLTK_TRACING_PATH}
fi

TARGETS=$(echo "$*" | grep -oP "((--target|-t)(\s+\S+)+)")

cmake -S ${ROOT_PATH} -B $BUILD_DIR
cmake --build $BUILD_DIR $TARGETS -- -j$(nproc)

