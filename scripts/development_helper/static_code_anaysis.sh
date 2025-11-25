#!/usr/bin/env bash
# Copyright (c) 2024, International Business Machines
# SPDX-License-Identifier: BSD-2-Clause-Patent
set -euo pipefail

# Trap Ctrl+C and kill all background jobs
cleanup() {
    echo -e "\nInterrupted. Killing background jobs..."
    jobs -p | xargs -r kill 2>/dev/null
    exit 130
}
trap cleanup SIGINT SIGTERM

# Ensure required tools exist
command -v jq > /dev/null || { echo "Error: jq is not installed."; exit 1; }
command -v clang-tidy > /dev/null || { echo "Error: clang-tidy is not installed."; exit 1; }

ROOT_PATH=$(git rev-parse --show-toplevel)
cd "${ROOT_PATH}"

cmake --preset unittests --fresh -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ 

FILTER="${1:-}"

static_analyse() {
    local file="$1"
    echo "Running clang-tidy on: $file"
    clang-tidy -checks='performance-*,bugprone-*' \
               -p=build "$file"
}

# Run clang-tidy on each file in compile_commands.json
while IFS= read -r file; do
    if [[ "$file" == *"/build/"* ]]; then
        continue
    fi
    file="$(realpath --relative-to="$ROOT_PATH" $file)"
    if [[ -n "$FILTER" && "$file" != *"$FILTER"* ]]; then
        continue
    fi
    static_analyse "$file" &
done < <(jq -r '.[].file' build/compile_commands.json)

wait