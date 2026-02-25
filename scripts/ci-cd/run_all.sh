#!/usr/bin/env bash
# Copyright (c) 2024, International Business Machines
# SPDX-License-Identifier: BSD-2-Clause-Patent

# CI Orchestrator: Run all CI steps
# Usage: ./scripts/ci-cd/run_all.sh [OPTIONS]
#
# This script runs the complete CI pipeline locally, identical to what runs on GitHub Actions.
# Each step can also be run independently using the individual step_*.sh scripts.
#
# Exit: 0 = all passed, 1 = any failed

set -euo pipefail

ROOT_PATH=$(git rev-parse --show-toplevel)
cd "${ROOT_PATH}"

# Options (can be set via environment or command line)
SKIP_STATIC_ANALYSIS=${CLLTK_SKIP_STATIC_ANALYSIS:-false}
SKIP_PACKAGE=${CLLTK_SKIP_PACKAGE:-false}
SKIP_MEMCHECK=${CLLTK_SKIP_MEMCHECK:-false}
CHECK_VERSION=${CLLTK_CHECK_VERSION:-false}
STOP_ON_FAILURE=${CLLTK_STOP_ON_FAILURE:-false}

usage() {
    echo "Usage: $0 [OPTIONS]"
    echo ""
    echo "Run the complete CI pipeline locally."
    echo ""
    echo "Options:"
    echo "  --skip-static-analysis    Skip static analysis step"
    echo "  --skip-package            Skip packaging step"
    echo "  --skip-memcheck           Skip memory check step"
    echo "  --check-version           Run version validation (for PRs)"
    echo "  --stop-on-failure         Stop immediately on first failure"
    echo "  --help                    Show this help message"
    echo ""
    echo "Environment variables:"
    echo "  CLLTK_SKIP_STATIC_ANALYSIS=true    Skip static analysis"
    echo "  CLLTK_SKIP_PACKAGE=true            Skip packaging"
    echo "  CLLTK_SKIP_MEMCHECK=true           Skip memory check"
    echo "  CLLTK_CHECK_VERSION=true           Run version validation"
    echo "  CLLTK_STOP_ON_FAILURE=true         Stop on first failure"
    echo ""
    echo "Examples:"
    echo "  $0                                  # Run full pipeline"
    echo "  $0 --skip-package                   # Skip packaging"
    echo "  $0 --stop-on-failure                # Stop on first error"
    echo "  CLLTK_SKIP_STATIC_ANALYSIS=true $0  # Skip static analysis via env"
    exit 0
}

while [[ $# -gt 0 ]]; do
    case $1 in
        --skip-static-analysis) SKIP_STATIC_ANALYSIS=true; shift ;;
        --skip-package) SKIP_PACKAGE=true; shift ;;
        --skip-memcheck) SKIP_MEMCHECK=true; shift ;;
        --check-version) CHECK_VERSION=true; shift ;;
        --stop-on-failure) STOP_ON_FAILURE=true; shift ;;
        --help) usage ;;
        *) echo "Unknown option: $1"; usage ;;
    esac
done

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

echo "========================================"
echo "CI Pipeline - Full Run"
echo "========================================"
echo "Options:"
echo "  Skip static analysis: $SKIP_STATIC_ANALYSIS"
echo "  Skip package:         $SKIP_PACKAGE"
echo "  Skip memcheck:        $SKIP_MEMCHECK"
echo "  Check version:        $CHECK_VERSION"
echo "  Stop on failure:      $STOP_ON_FAILURE"
echo ""

FAILED_STEPS=()
PASSED_STEPS=()

run_step() {
    local name="$1"
    local script="$2"
    shift 2
    
    echo ""
    echo "========================================"
    echo ">>> Running: $name"
    echo "========================================"
    
    if "$SCRIPT_DIR/$script" "$@"; then
        echo "<<< $name: PASSED"
        PASSED_STEPS+=("$name")
        return 0
    else
        echo "<<< $name: FAILED"
        FAILED_STEPS+=("$name")
        if $STOP_ON_FAILURE; then
            echo ""
            echo "Stopping due to --stop-on-failure"
            exit 1
        fi
        return 1
    fi
}

# =============================================================================
# Pipeline Steps
# =============================================================================

# Step 1: Version check (PR only)
if $CHECK_VERSION; then
    run_step "Version Check" "step_version.sh" || true
fi

# Step 2: Format check
run_step "Format Check" "step_format.sh" || true

# Step 3: Build (required for subsequent steps)
if ! run_step "Build" "step_build.sh"; then
    echo ""
    echo "Build failed - cannot continue with tests"
    if ! $STOP_ON_FAILURE; then
        echo "Skipping remaining steps that require build artifacts"
    fi
else
    # Steps that depend on successful build
    
    # Step 4: Tests
    run_step "Tests" "step_test.sh" || true

    # Step 5: Memory check
    if ! $SKIP_MEMCHECK; then
        run_step "Memory Check" "step_memcheck.sh" || true
    fi

    # Step 6: Static analysis
    if ! $SKIP_STATIC_ANALYSIS; then
        run_step "Static Analysis" "step_static_analysis.sh" --all || true
    fi

    # Step 7: Package
    if ! $SKIP_PACKAGE; then
        run_step "Package" "step_package.sh" || true

        # Step 8: Package Validation
        run_step "Package Validate" "step_package_validate.sh" --skip-srpm-rebuild || true
    fi
fi

# =============================================================================
# Summary
# =============================================================================

echo ""
echo "========================================"
echo "CI Pipeline Summary"
echo "========================================"
echo ""

if [ ${#PASSED_STEPS[@]} -gt 0 ]; then
    echo "PASSED steps:"
    for step in "${PASSED_STEPS[@]}"; do
        echo "  - $step"
    done
fi

if [ ${#FAILED_STEPS[@]} -gt 0 ]; then
    echo ""
    echo "FAILED steps:"
    for step in "${FAILED_STEPS[@]}"; do
        echo "  - $step"
    done
    echo ""
    echo "========================================"
    echo "CI Pipeline: FAILED"
    echo "========================================"
    exit 1
else
    echo ""
    echo "========================================"
    echo "CI Pipeline: PASSED"
    echo "========================================"
    exit 0
fi
