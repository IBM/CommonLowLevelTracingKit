#!/usr/bin/env bash
# Copyright (c) 2024, International Business Machines
# SPDX-License-Identifier: BSD-2-Clause-Patent

# CI Step: Package Validation
# Usage: ./scripts/ci-cd/step_package_validate.sh [--skip-srpm-rebuild]
# Exit: 0 = success, 1 = validation failed
#
# Validates RPM packages produced by step_package.sh:
#   - Level 1: RPM artifact validation (metadata, files, deps, scriptlets)
#   - Level 2: Install smoke tests (cmake --install + verify)
#   - Level 2+: Consumer tests (find_package, pkg-config)
#   - Level 3: SRPM rebuild (can be skipped with --skip-srpm-rebuild)

set -euo pipefail

ROOT_PATH=$(git rev-parse --show-toplevel)
cd "${ROOT_PATH}"

SKIP_SRPM_REBUILD=${CLLTK_SKIP_SRPM_REBUILD:-false}

while [[ $# -gt 0 ]]; do
    case $1 in
        --skip-srpm-rebuild) SKIP_SRPM_REBUILD=true; shift ;;
        *) echo "Unknown option: $1"; exit 1 ;;
    esac
done

echo "========================================"
echo "CI Step: Package Validation"
echo "========================================"

PACKAGES_DIR="${BUILD_DIR:-./build}/packages"

# Check if RPMs exist, build if missing
if [ ! -d "$PACKAGES_DIR" ] || [ -z "$(ls -A "$PACKAGES_DIR"/*.rpm 2>/dev/null)" ]; then
    echo "RPM packages not found in $PACKAGES_DIR, building..."
    if ! cmake --workflow --preset rpms; then
        echo "FAILED: Could not build RPM packages"
        exit 1
    fi
fi

echo "Found RPM packages in $PACKAGES_DIR:"
ls -la "$PACKAGES_DIR"/*.rpm 2>/dev/null || true
echo ""

# Run packaging tests
# Note: -t . is required so that relative imports (from .helpers.rpm) resolve correctly
if $SKIP_SRPM_REBUILD; then
    echo "Running packaging tests (Levels 1-2+, skipping SRPM rebuild)..."
    TEST_PATTERN='test_rpm_*.py test_consumer_*.py'
    # Run each pattern separately since unittest discover takes one -p
    python3 -m unittest discover -v -t . -s ./tests/packaging -p 'test_rpm_*.py' || { echo "FAILED: Packaging tests failed"; exit 1; }
    python3 -m unittest discover -v -t . -s ./tests/packaging -p 'test_consumer_*.py' || { echo "FAILED: Packaging tests failed"; exit 1; }
else
    echo "Running packaging tests (Levels 1-3, including SRPM rebuild)..."
    if ! python3 -m unittest discover -v -t . -s ./tests/packaging -p 'test_*.py'; then
        echo "FAILED: Packaging tests failed"
        exit 1
    fi
fi

echo ""
echo "========================================"
echo "PASSED: Package validation complete"
exit 0
