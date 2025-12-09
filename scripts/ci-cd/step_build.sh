#!/usr/bin/env bash
# Copyright (c) 2024, International Business Machines
# SPDX-License-Identifier: BSD-2-Clause-Patent

# CI Step: Configure and Build
# Usage: ./scripts/ci-cd/step_build.sh [--preset <name>] [--clean]
# Exit: 0 = success, 1 = build failed

set -euo pipefail

ROOT_PATH=$(git rev-parse --show-toplevel)
cd "${ROOT_PATH}"

# Defaults
PRESET="${CLLTK_BUILD_PRESET:-unittests}"
CLEAN=false

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --preset) PRESET="$2"; shift 2 ;;
        --clean) CLEAN=true; shift ;;
        *) echo "Unknown option: $1"; exit 1 ;;
    esac
done

echo "========================================"
echo "CI Step: Build"
echo "========================================"
echo "Preset: $PRESET"
echo ""

BUILD_DIR="${BUILD_DIR:-./build/}"

# Clean if requested
if $CLEAN && [ -d "$BUILD_DIR" ]; then
    echo "Cleaning build directory..."
    rm -rf "${BUILD_DIR:?}"/*
fi

# Configure
echo "Configuring..."
if ! cmake --preset "$PRESET" --fresh; then
    echo "FAILED: Configuration failed"
    exit 1
fi

# Build
echo ""
echo "Building..."
if ! cmake --build --preset "$PRESET" --clean-first; then
    echo "FAILED: Build failed"
    exit 1
fi

echo ""
echo "PASSED: Build successful"
exit 0
