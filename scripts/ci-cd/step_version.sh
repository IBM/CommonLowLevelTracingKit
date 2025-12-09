#!/usr/bin/env bash
# Copyright (c) 2024, International Business Machines
# SPDX-License-Identifier: BSD-2-Clause-Patent

# CI Step: Version Step Validation (for PRs)
# Usage: ./scripts/ci-cd/step_version.sh
# Exit: 0 = valid, 1 = missing version step

set -euo pipefail

ROOT_PATH=$(git rev-parse --show-toplevel)
cd "${ROOT_PATH}"

echo "========================================"
echo "CI Step: Version Validation"
echo "========================================"

# Delegate to existing script
if ./scripts/ci-cd/check_version_step.sh; then
    echo "PASSED: Version step is valid"
    exit 0
else
    echo "FAILED: Version step missing"
    exit 1
fi
