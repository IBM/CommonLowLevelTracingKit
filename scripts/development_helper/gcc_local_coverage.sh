#!/usr/bin/env bash
# Copyright (c) 2024, International Business Machines
# SPDX-License-Identifier: BSD-2-Clause-Patent

set -euo pipefail

echo "< following output from bash file: $0 >"
echo

ROOT_PATH=$(git rev-parse --show-toplevel)
echo "ROOT_PATH=${ROOT_PATH}"
cd "${ROOT_PATH}"

# Generate build files with CMake
rm -rf build/*
cmake --preset unittests --fresh -DENABLE_CODE_COVERAGE=ON
cmake --build --preset unittests --target clean -- -j32
cmake --build --preset unittests --target all -- -j32
lcov --zerocounters --directory build
ctest --preset unittests $@

rm -rf build/coverage
mkdir -p build/coverage/{gcovr,lcov}
gcovr --root $ROOT_PATH \
    --exclude ".*/?build/.*" \
    --exclude ".*/?command_line_tool/.*" \
    --exclude ".*/?examples/.*" \
    --exclude ".*/?tests/.*" \
    --exclude-lines-by-pattern ".*extern.*" \
    --exclude-lines-by-pattern ".*ERROR_AND_EXIT.*" \
    --exclude-noncode-lines \
    --exclude-function-lines \
    --exclude-unreachable-branches \
    --exclude-throw-branches \
    --merge-mode-functions=separate \
    --merge-mode-conditions=fold \
    --html --html-details -o build/coverage/gcovr/coverage.html \
    --lcov build/coverage/lcov/coverage_raw.info \


lcov --remove build/coverage/lcov/coverage_raw.info '/usr/* '\
     --demangle-cpp \
    --branch-coverage \
    --ignore-errors inconsistent,inconsistent \
    --ignore-errors mismatch,mismatch \
    --ignore-errors source,source \
    --ignore-errors unused \
    -o build/coverage/lcov/coverage.info

genhtml build/coverage/lcov/coverage.info \
    --output-directory build/coverage/lcov/ \
    --ignore-errors inconsistent,inconsistent \
    --ignore-errors mismatch,mismatch \
    --ignore-errors source,source \
    --ignore-errors unused \
    --no-checksum \
    --demangle-cpp \
    --branch-coverage \
    --function-coverage \
    --legend \
    --title "CommonLowLevelTracingKit" \
    --show-details

echo "build/coverage/gcovr/coverage.html"
echo build/coverage/lcov/index.html
