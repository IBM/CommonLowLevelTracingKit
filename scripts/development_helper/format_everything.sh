#!/usr/bin/env bash
# Copyright (c) 2024, International Business Machines
# SPDX-License-Identifier: BSD-2-Clause-Patent

ROOT_PATH=$(git rev-parse --show-toplevel)
cd ${ROOT_PATH}

format() {
  local -r file="$1"
  clang-format -style=file -i "$file"
  if [ $? -ne 0 ]; then
    echo "formating failed for $file"
    exit 1
  fi
}
files=$(find ./ -type f \( -not -path "*/build/*" -and -not -path "*/build_kernel/*" \) -regex '.*\.\(hpp\|h\|cpp\|c\)$')
for file in $files; do
  format "$file" &
done
wait
