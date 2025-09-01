#!/usr/bin/env bash
# Copyright (c) 2024, International Business Machines
# SPDX-License-Identifier: BSD-2-Clause-Patent

set -euo pipefail

echo "< following output from bash file: $0 >"
echo

ROOT_PATH=$(git rev-parse --show-toplevel)
echo "ROOT_PATH=${ROOT_PATH}"
cd "${ROOT_PATH}"

# Generate build files with CMake
rm -rf build
cmake --preset unittests --fresh -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -DCLLTK_EXAMPLES=OFF -DENABLE_CODE_COVERAGE=ON
cmake --build --preset unittests --target all -- -j32
ctest --preset unittests

rm -rf build/profraw
MAP_FILE="build/build_hash_map.txt"
>"$MAP_FILE"
declare -A build_hash_map
for exe in $(find "build" -type f -executable); do
    if [[ $exe == *CMakeFiles* ]]; then continue; fi
    if [[ $exe == *git* ]]; then continue; fi
    if [[ $exe == *py* ]]; then continue; fi
    hash="$(llvm-readelf -n "$exe" | grep -oP "(?<=Build ID: )([0-9a-z]+)$")"
    build_hash_map["$hash"]="$exe"
    echo "$hash $exe" >>"$MAP_FILE"
    mkdir -p "build/profraw/$hash"
done

move_prof_file() {
    local -r profraw="$1"
    local -r hash="$(llvm-profdata show --binary-ids $profraw | tail -n1)"
    mkdir -p "build/profraw/$hash"
    cp "$profraw" "build/profraw/$hash"
}
export -f move_prof_file
find build -type f -name '*.profraw' -print0 |
    xargs -0 -P8 -I{} bash -c 'move_prof_file "$0"' {}

mkdir -p build/coverage
rm -rf build/coverage/*

output=()
for hash in "${!build_hash_map[@]}"; do
    exe="${build_hash_map[$hash]}"
    if [[ $exe != build/tests/* ]]; then continue; fi
    if [[ $exe == build/tests/robot* ]]; then continue; fi

    dir="build/profraw/$hash"
    if [ -z "$(find $dir -mindepth 1 -print -quit -name "*.profraw")" ]; then
        output+=("$exe empty($dir)")
    else
        name="$(basename "$exe")"
        llvm-profdata merge -sparse $dir/*.profraw -o $dir/merged.profdata

        # for single exe report
        llvm-cov show $exe \
            -instr-profile=$dir/merged.profdata \
            -format=html \
            --show-directory-coverage \
            --show-branches=percent \
            --show-expansions \
            -output-dir=build/coverage/$name \
            -Xdemangler=c++filt \
            -show-line-counts-or-regions \
            --ignore-filename-regex='(_deps|test)' \
            -show-instantiations
        output+=("$name build/coverage/$name/index.html")

        # for total report
        llvm-cov export \
            -format=lcov \
            -instr-profile=$dir/merged.profdata \
            --ignore-filename-regex='(_deps|test|/build/)' \
            $exe \
            >$dir/combined-coverage.info
    fi
    echo "$exe" >"$dir/name"
done

# merge all to one report
lcov \
    -j $(nproc --ignore=1) \
    --branch-coverage \
    --base-director $ROOT_PATH \
    $(find build/profraw -name 'combined-coverage.info' -exec printf -- '-a %s ' {} \;) \
    -o build/profraw/merged.info

# create one total report
genhtml build/profraw/merged.info \
    --show-details \
    --prefix $ROOT_PATH \
    --output-directory build/coverage/total \
    --function-coverage \
    --demangle-cpp \
    --highlight \
    --legend \
    --sort \
    --quiet

printf "%s\n" "${output[@]}" | column -t | sort
echo ""
echo "total - build/coverage/total/index.html"
rm -rf build/profraw
