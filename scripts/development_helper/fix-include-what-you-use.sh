#!/usr/bin/env bash
# Copyright (c) 2024, International Business Machines
# SPDX-License-Identifier: BSD-2-Clause-Patent

ROOT_PATH=$(git rev-parse --show-toplevel)
cd "${ROOT_PATH}"

FILTER="$1"

my_iwyu() {
    local file="$1"
    echo "Running iwyu on: $file"
    iwyu_tool.py -p build "$file" | fix_includes.py
}

for file in $(jq -r '.[].file' build/compile_commands.json); do
    if [[ "$file" == *"/build/"* ]]; then
        continue
    fi

    if [[ -n "$FILTER" && "$file" != *"$FILTER"* ]]; then
        continue
    fi

    my_iwyu "$file" &
done
wait
