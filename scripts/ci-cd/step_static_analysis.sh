#!/usr/bin/env bash
# Copyright (c) 2024, International Business Machines
# SPDX-License-Identifier: BSD-2-Clause-Patent

# CI Step: Static Analysis (clang-tidy, cppcheck, iwyu)
# Usage: ./scripts/ci-cd/step_static_analysis.sh [--clang-tidy] [--cppcheck] [--iwyu] [--fix] [--werror]
# Exit: 0 = success, 1 = issues found

set -euo pipefail

# Trap Ctrl+C and kill all background jobs
cleanup() {
    echo -e "\nInterrupted. Killing background jobs..."
    jobs -p | xargs -r kill 2>/dev/null
    exit 130
}
trap cleanup SIGINT SIGTERM

ROOT_PATH=$(git rev-parse --show-toplevel)
cd "${ROOT_PATH}"

BUILD_DIR="${BUILD_DIR:-build}"
COMPILE_COMMANDS="${BUILD_DIR}/compile_commands.json"

# Options
RUN_CLANG_TIDY=false
RUN_CPPCHECK=false
RUN_IWYU=false
WERROR=false
FIX=false
FILTER=""

usage() {
    echo "Usage: $0 [OPTIONS]"
    echo ""
    echo "Static analysis step for CI pipeline."
    echo ""
    echo "Options:"
    echo "  --clang-tidy    Run clang-tidy static analysis"
    echo "  --cppcheck      Run cppcheck static analysis"
    echo "  --iwyu          Run include-what-you-use analysis"
    echo "  --all           Run clang-tidy and cppcheck"
    echo "  --fix           Apply automatic fixes (clang-tidy only)"
    echo "  --werror        Treat warnings as errors"
    echo "  --filter PAT    Only analyze files matching pattern"
    echo "  --help          Show this help message"
    echo ""
    echo "Examples:"
    echo "  $0 --clang-tidy                    # Run clang-tidy only"
    echo "  $0 --all --werror                  # Run all, fail on warnings"
    echo "  $0 --clang-tidy --fix              # Run and auto-fix issues"
    echo "  $0 --clang-tidy --filter decoder   # Only check decoder files"
    exit 0
}

while [[ $# -gt 0 ]]; do
    case $1 in
        --clang-tidy) RUN_CLANG_TIDY=true; shift ;;
        --cppcheck) RUN_CPPCHECK=true; shift ;;
        --iwyu) RUN_IWYU=true; shift ;;
        --all) RUN_CLANG_TIDY=true; RUN_CPPCHECK=true; shift ;;
        --fix) FIX=true; shift ;;
        --werror) WERROR=true; shift ;;
        --filter) FILTER="$2"; shift 2 ;;
        --help) usage ;;
        *) echo "Unknown option: $1"; usage ;;
    esac
done

# Default to all if nothing specified
if ! $RUN_CLANG_TIDY && ! $RUN_CPPCHECK && ! $RUN_IWYU; then
    RUN_CLANG_TIDY=true
    RUN_CPPCHECK=true
fi

echo "========================================"
echo "CI Step: Static Analysis"
echo "========================================"
echo "clang-tidy: $RUN_CLANG_TIDY"
echo "cppcheck:   $RUN_CPPCHECK"
echo "iwyu:       $RUN_IWYU"
echo "werror:     $WERROR"
echo "fix:        $FIX"
echo "filter:     ${FILTER:-<none>}"
echo ""

# Check prerequisites
# Directories and patterns excluded from static analysis:
# - build/              : Build artifacts
# - build_kernel/       : Kernel build artifacts  
# - _deps/              : CMake FetchContent dependencies (e.g., googletest)
# - /usr/*              : System includes
# - boost/              : Boost library headers

check_prerequisites() {
    if [[ ! -f "$COMPILE_COMMANDS" ]]; then
        echo "compile_commands.json not found. Building first..."
        cmake --preset unittests --fresh
        cmake --build --preset unittests
    fi

    if $RUN_CLANG_TIDY; then
        command -v clang-tidy > /dev/null || { echo "Error: clang-tidy not installed"; exit 1; }
        echo "clang-tidy: $(clang-tidy --version | head -1)"
    fi

    if $RUN_CPPCHECK; then
        command -v cppcheck > /dev/null || { echo "Error: cppcheck not installed"; exit 1; }
        echo "cppcheck: $(cppcheck --version)"
    fi

    if $RUN_IWYU; then
        command -v include-what-you-use > /dev/null || { echo "Warning: include-what-you-use not installed"; RUN_IWYU=false; }
    fi

    command -v jq > /dev/null || { echo "Error: jq not installed"; exit 1; }
    echo ""
}

# Get source files from compile_commands.json
get_source_files() {
    jq -r '.[].file' "$COMPILE_COMMANDS" | while read -r file; do
        # Skip build directory files
        [[ "$file" == *"/build/"* ]] && continue
        [[ "$file" == *"/build_kernel/"* ]] && continue
        # Skip external dependencies (boost, system includes, etc.)
        [[ "$file" == *"/boost/"* ]] && continue
        [[ "$file" == /usr/* ]] && continue
        [[ "$file" == *"_deps/"* ]] && continue
        # Apply filter if specified
        if [[ -n "$FILTER" && "$file" != *"$FILTER"* ]]; then
            continue
        fi
        realpath --relative-to="$ROOT_PATH" "$file" 2>/dev/null || echo "$file"
    done | sort -u
}

ERRORS_FOUND=0

# =============================================================================
# clang-tidy
# =============================================================================
run_clang_tidy() {
    echo "----------------------------------------"
    echo "Running clang-tidy..."
    echo "----------------------------------------"

    local files
    files=$(get_source_files)
    local file_count
    file_count=$(echo "$files" | wc -l)
    
    echo "Analyzing $file_count files..."

    local tidy_args=("-p=$BUILD_DIR")
    
    if $FIX; then
        tidy_args+=("--fix" "--fix-errors")
    fi

    if $WERROR; then
        tidy_args+=("--warnings-as-errors=*")
    fi

    local failed_files=()
    local current=0

    for file in $files; do
        ((current++)) || true
        echo "[$current/$file_count] $file"
        
        if ! clang-tidy "${tidy_args[@]}" "$file" 2>&1; then
            failed_files+=("$file")
        fi
    done

    echo ""
    if [[ ${#failed_files[@]} -gt 0 ]]; then
        echo "clang-tidy: FAILED (${#failed_files[@]} files with issues)"
        ERRORS_FOUND=1
    else
        echo "clang-tidy: PASSED"
    fi
}

# =============================================================================
# cppcheck
# =============================================================================
run_cppcheck() {
    echo "----------------------------------------"
    echo "Running cppcheck..."
    echo "----------------------------------------"

    local cppcheck_args=(
        "--project=$COMPILE_COMMANDS"
        "--enable=warning,performance,portability"
        "--inline-suppr"
        "--suppress=missingIncludeSystem"
        "--suppress=unmatchedSuppression"
        "--template={file}:{line}: [{severity}] {id}: {message}"
        "--quiet"
    )

    if $WERROR; then
        cppcheck_args+=("--error-exitcode=1")
    fi

    # Exclude build directory and external dependencies
    cppcheck_args+=("-i${ROOT_PATH}/build")
    cppcheck_args+=("-i${ROOT_PATH}/build_kernel")
    # Suppress boost-related warnings
    cppcheck_args+=("--suppress=*:*boost*")
    cppcheck_args+=("--suppress=*:/usr/include/*")
    cppcheck_args+=("--suppress=*:/usr/local/include/*")

    if ! cppcheck "${cppcheck_args[@]}" 2>&1; then
        echo "cppcheck: FAILED"
        ERRORS_FOUND=1
    else
        echo "cppcheck: PASSED"
    fi
}

# =============================================================================
# include-what-you-use
# =============================================================================
run_iwyu() {
    echo "----------------------------------------"
    echo "Running include-what-you-use..."
    echo "----------------------------------------"
    echo "(This is informational only - results are suggestions)"
    echo ""

    local files
    files=$(get_source_files)

    for file in $files; do
        # Only C++ files for iwyu
        [[ "$file" != *.cpp && "$file" != *.hpp ]] && continue
        
        echo "Checking: $file"
        include-what-you-use -p "$BUILD_DIR" "$file" 2>&1 || true
    done

    echo ""
    echo "iwyu: COMPLETE (informational only)"
}

# =============================================================================
# Main
# =============================================================================
main() {
    check_prerequisites

    $RUN_CLANG_TIDY && run_clang_tidy
    $RUN_CPPCHECK && run_cppcheck
    $RUN_IWYU && run_iwyu

    echo ""
    echo "========================================"
    if [[ $ERRORS_FOUND -eq 0 ]]; then
        echo "PASSED: Static analysis complete"
        exit 0
    else
        echo "FAILED: Static analysis found issues"
        exit 1
    fi
}

main
