#!/usr/bin/env bash
# Copyright (c) 2024, International Business Machines
# SPDX-License-Identifier: BSD-2-Clause-Patent

echo
echo \< following output from bash file: $0 \>
echo

ROOT_PATH=$(git rev-parse --show-toplevel)
echo "ROOT_PATH=${ROOT_PATH}"
cd ${ROOT_PATH}

BUILD_DIR=${BUILD_DIR:=./build/}
BUILD_DIR=$(realpath $BUILD_DIR)/
echo BUILD_DIR="$BUILD_DIR"

if [ -n "${CLLTK_TRACING_PATH}" ]; then
    rm -f ${CLLTK_TRACING_PATH}/*
fi

ctest --test-dir "$BUILD_DIR/" --output-on-failure
if [ $? -eq 0 ];
then
    echo "c/c++ test ok"
else
    echo "c/c++ test failed"
    exit 1 
fi

python3 -m unittest discover -v -s "./tests/python_tests" -p '*.py'
if [ $? -eq 0 ];
then
    echo "python unittest test ok"
else
    echo "python unittest  test failed"
    exit 1 
fi

python3 -m unittest discover -v -s "./tests/integration_tests" -p 'test_*.py'
if [ $? -eq 0 ];
then
    echo "python integration test ok"
else
    echo "python integration test failed"
    exit 1 
fi

echo "all test are ok"
exit 0