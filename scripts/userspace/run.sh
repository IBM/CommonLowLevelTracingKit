#!/usr/bin/env bash
# Copyright (c) 2024, International Business Machines
# SPDX-License-Identifier: BSD-2-Clause-Patent

# searches for exe in build and runs it if found with args 

echo
echo \< following output from bash file: $0 \>
echo

ROOT_PATH=$(git rev-parse --show-toplevel)
echo "ROOT_PATH=${ROOT_PATH}"
OLD_CWD=$(pwd)
cd ${ROOT_PATH}

BUILD_DIR=${BUILD_DIR:-./build/}
BUILD_DIR=$(realpath $BUILD_DIR)/
echo BUILD_DIR="$BUILD_DIR"

EXE_NAME=$1
shift
EXE_ARGS=$*

FOUND_FILE=$(find $BUILD_DIR -type f -executable -name "$EXE_NAME" -print -quit)
echo FOUND_FILE = $FOUND_FILE

if [[ -z "${CLLTK_TRACING_PATH}" ]] ; then
    mkdir -p /tmp/traces
    export CLLTK_TRACING_PATH=/tmp/traces/
fi

if [ -n "$FOUND_FILE" ]; then
    cd $OLD_CWD
    $FOUND_FILE $EXE_ARGS
else
    echo "File '$EXE_NAME' not found in $BUILD_DIR"
fi
