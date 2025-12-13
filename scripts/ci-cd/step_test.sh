#!/usr/bin/env bash
# Copyright (c) 2024, International Business Machines
# SPDX-License-Identifier: BSD-2-Clause-Patent

# CI Step: Run Tests
# Usage: ./scripts/ci-cd/step_test.sh
# Exit: 0 = success, 1 = tests failed

set -euo pipefail

ROOT_PATH=$(git rev-parse --show-toplevel)
cd "${ROOT_PATH}"

echo "========================================"
echo "CI Step: Tests"
echo "========================================"

BUILD_DIR="${BUILD_DIR:-./build/}"
BUILD_DIR=$(realpath "$BUILD_DIR")/

# Clear traces if path is set
if [ -n "${CLLTK_TRACING_PATH:-}" ]; then
    rm -f "${CLLTK_TRACING_PATH}"/* 2>/dev/null || true
fi

# C/C++ tests
echo "Running C/C++ tests..."
if ! ctest --test-dir "$BUILD_DIR" --output-on-failure; then
    echo "FAILED: C/C++ tests failed"
    exit 1
fi
echo "C/C++ tests: PASSED"
echo ""

# Python tests
echo "Running Python tests..."
if ! python3 -m unittest discover -v -s "./tests" -p 'test_*.py'; then
    echo "FAILED: Python tests failed"
    exit 1
fi
echo "Python tests: PASSED"
echo ""

echo "========================================"
echo "PASSED: All tests successful"
exit 0
