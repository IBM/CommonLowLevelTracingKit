#!/usr/bin/env bash
# Copyright (c) 2024, International Business Machines
# SPDX-License-Identifier: BSD-2-Clause-Patent

# CI Step: Format Check
# Usage: ./scripts/ci-cd/step_format.sh
# Exit: 0 = success, 1 = formatting issues found

set -euo pipefail

ROOT_PATH=$(git rev-parse --show-toplevel)
cd "${ROOT_PATH}"

echo "========================================"
echo "CI Step: Format Check"
echo "========================================"
echo "clang-format = $(clang-format --version)"
echo ""

unformatted_files=()

check_file() {
    local file_name="$1"
    if ! clang-format -style=file --dry-run --output-replacements-xml --Werror "$file_name" > /dev/null 2>&1; then
        unformatted_files+=("$file_name")
        echo "NOT FORMATTED: $file_name"
    fi
}

# Find all C/C++ source files
files=$(find ./ -type f \
    \( -not -path "*/build/*" -and -not -path "*/build_kernel/*" -and -not -path "*.mod.c" \) \
    -regex '.*\.\(hpp\|h\|cpp\|c\)$')

for file in $files; do
    check_file "$file"
done

echo ""
if [ ${#unformatted_files[@]} -ne 0 ]; then
    echo "FAILED: ${#unformatted_files[@]} files need formatting"
    echo ""
    echo "To fix, run:"
    echo "  ./scripts/development_helper/format_everything.sh"
    exit 1
else
    echo "PASSED: All files are properly formatted"
    exit 0
fi
