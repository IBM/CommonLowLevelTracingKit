# Copyright (c) 2024, International Business Machines
# SPDX-License-Identifier: BSD-2-Clause-Patent


#!/usr/bin/env bash


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
    valgrind --tool=memcheck --leak-check=full --show-leak-kinds=all --track-origins=yes --errors-for-leak-kinds=all --error-exitcode=1 -s -- $FOUND_FILE $EXE_ARGS
    exit $?
else
    echo "File '$EXE_NAME' not found in $BUILD_DIR"
    exit 1
fi


