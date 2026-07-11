#!/usr/bin/env bash
# Copyright (c) 2024, International Business Machines
# SPDX-License-Identifier: BSD-2-Clause-Patent

# CI Step: build and run the test suite under a sanitizer.
# Usage: ./scripts/ci-cd/step_sanitizers.sh <asan|tsan>
# Exit: 0 = clean, 1 = sanitizer findings or build failure

set -euo pipefail

ROOT_PATH=$(git rev-parse --show-toplevel)
cd "${ROOT_PATH}"

KIND="${1:-asan}"
case "$KIND" in
    asan) PRESET="unittests-asan" ;;
    tsan) PRESET="unittests-tsan" ;;
    *) echo "usage: $0 <asan|tsan>"; exit 1 ;;
esac

echo "========================================"
echo "CI Step: Sanitizer ($KIND)"
echo "========================================"

# halt_on_error=1 makes the first finding fail the run instead of scrolling past.
# Do NOT set abort_on_error for ASan: the death tests expect ASan's default
# exit(1) on a caught error (EXPECT_EXIT(ExitedWithCode(1))); abort() would
# raise SIGABRT and fail those tests.
export ASAN_OPTIONS="detect_leaks=1:halt_on_error=1:${ASAN_OPTIONS:-}"
export UBSAN_OPTIONS="halt_on_error=1:print_stacktrace=1:${UBSAN_OPTIONS:-}"
# TSan understands pthreads but not our shared-memory futex mutexes; a
# suppression file (if present) keeps known-safe patterns from failing CI.
export TSAN_OPTIONS="halt_on_error=1:${TSAN_OPTIONS:-}"
if [[ "$KIND" == "tsan" && -f "$ROOT_PATH/scripts/ci-cd/tsan.suppressions" ]]; then
    export TSAN_OPTIONS="suppressions=$ROOT_PATH/scripts/ci-cd/tsan.suppressions:$TSAN_OPTIONS"
fi

echo "Configuring ($PRESET)..."
cmake --preset "$PRESET" --fresh

echo "Building..."
cmake --build --preset "$PRESET" -- -j"$(nproc)"

echo "Testing under $KIND..."
if ! ctest --preset "$PRESET" --output-on-failure; then
    echo "FAILED: $KIND sanitizer found issues"
    exit 1
fi

echo "PASSED: $KIND sanitizer clean"
exit 0
