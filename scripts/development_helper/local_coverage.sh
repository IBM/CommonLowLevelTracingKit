#!/usr/bin/env bash
# Copyright (c) 2024, International Business Machines
# SPDX-License-Identifier: BSD-2-Clause-Patent

echo
echo \< following output from bash file: $0 \>
echo

ROOT_PATH=$(git rev-parse --show-toplevel)
echo "ROOT_PATH=${ROOT_PATH}"
cd ${ROOT_PATH}

cmake --workflow --preset unittests

mkdir -p ./build/coverage/
lcov --capture --directory ./build/ --output-file ./build/coverage/coverage_all.info
lcov --remove ./build/coverage/coverage_all.info '/usr/*' "$(pwd)/build/_deps/*" "*.tests" --output-file ./build/coverage/coverage.info
genhtml ./build/coverage/coverage.info --output-directory ./build/coverage
