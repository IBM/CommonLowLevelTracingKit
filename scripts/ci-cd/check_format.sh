#!/usr/bin/env bash
# Copyright (c) 2024, International Business Machines
# SPDX-License-Identifier: BSD-2-Clause-Patent

ROOT_PATH=$(git rev-parse --show-toplevel)
cd ${ROOT_PATH}

echo "clang-format = $( clang-format --version )"

unformatted_files=()
function check {
    file_name="$1"
    clang-format -style=file --dry-run --output-replacements-xml --Werror "$file_name"
    if [ $? -ne 0 ]; then
      unformatted_files+=("$file_name")
      printf "%-15s\n" "not formatted: $file_name"
    fi
}

files=$(find ./ -type f \( -not -path "*/build/*" -and -not -path "*/build_kernel/*" -and -not -path "*.mod.c" \) -regex '.*\.\(hpp\|h\|cpp\|c\)$')
for file in $files;
do
  check "$file"
done

if [ ${#unformatted_files[@]} -ne 0 ]; then
  exit 1
else
  echo "everything is formatted"
  exit 0
fi
