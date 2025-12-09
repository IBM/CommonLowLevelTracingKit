#!/usr/bin/env bash
# Copyright (c) 2024, International Business Machines
# SPDX-License-Identifier: BSD-2-Clause-Patent

# CI Step: Memory Check (Valgrind)
# Usage: ./scripts/ci-cd/step_memcheck.sh [--examples <example1> <example2> ...]
# Exit: 0 = success, 1 = memory issues found

set -euo pipefail

ROOT_PATH=$(git rev-parse --show-toplevel)
cd "${ROOT_PATH}"

echo "========================================"
echo "CI Step: Memory Check (Valgrind)"
echo "========================================"

BUILD_DIR="${BUILD_DIR:-./build/}"
BUILD_DIR=$(realpath "$BUILD_DIR")/

# Default examples to check
EXAMPLES=(
    example-simple_c
    example-complex_c
    example-extreme_c
    example-gen_format_c
    example-simple_cpp
    example-complex_cpp
    example-with_libraries
    example-process_threads
)

# Override with arguments if provided
if [[ $# -gt 0 && "$1" == "--examples" ]]; then
    shift
    EXAMPLES=("$@")
fi

# Setup trace path
if [[ -z "${CLLTK_TRACING_PATH:-}" ]]; then
    mkdir -p /tmp/traces
    export CLLTK_TRACING_PATH=/tmp/traces/
fi

failed_examples=()

for example in "${EXAMPLES[@]}"; do
    echo ""
    echo "Checking: $example"
    
    FOUND_FILE=$(find "$BUILD_DIR" -type f -executable -name "$example" -print -quit)
    
    if [ -z "$FOUND_FILE" ]; then
        echo "  WARNING: $example not found in $BUILD_DIR"
        continue
    fi
    
    if ! valgrind --tool=memcheck \
                  --leak-check=full \
                  --show-leak-kinds=all \
                  --track-origins=yes \
                  --errors-for-leak-kinds=all \
                  --error-exitcode=1 \
                  -s \
                  -- "$FOUND_FILE" 2>&1; then
        failed_examples+=("$example")
        echo "  FAILED: Memory issues in $example"
    else
        echo "  PASSED: $example"
    fi
done

echo ""
echo "========================================"
if [ ${#failed_examples[@]} -ne 0 ]; then
    echo "FAILED: Memory issues found in: ${failed_examples[*]}"
    exit 1
else
    echo "PASSED: No memory issues found"
    exit 0
fi
