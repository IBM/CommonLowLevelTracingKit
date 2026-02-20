#!/usr/bin/env bash
# Copyright (c) 2024, International Business Machines
# SPDX-License-Identifier: BSD-2-Clause-Patent

# CI Step: Package (RPM)
# Usage: ./scripts/ci-cd/step_package.sh
# Exit: 0 = success, 1 = packaging failed

set -euo pipefail

ROOT_PATH=$(git rev-parse --show-toplevel)
cd "${ROOT_PATH}"

echo "========================================"
echo "CI Step: Packaging"
echo "========================================"

# Run the rpms workflow
echo "Building RPM packages..."
if ! cmake --workflow --preset rpms; then
    echo "FAILED: RPM workflow failed"
    exit 1
fi

# Build SRPM from real source tree
echo ""
echo "Building SRPM..."
if ! cmake --build --preset rpm --target srpm; then
    echo "FAILED: SRPM build failed"
    exit 1
fi

echo ""
echo "PASSED: Packaging complete"
exit 0
