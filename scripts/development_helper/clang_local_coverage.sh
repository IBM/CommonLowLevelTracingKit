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
ctest --preset unittests  $@

rm -rf build/coverage
mkdir -p build/coverage/{gcovr,lcov}
gcovr --root $ROOT_PATH \
    --gcov-executable "llvm-cov gcov" \
    --exclude ".*/?build/.*" \
    --exclude ".*/?command_line_tool/.*" \
    --exclude ".*/?examples/.*" \
    --exclude ".*/?tests/.*" \
    --exclude-lines-by-pattern ".*extern.*" \
    --exclude-lines-by-pattern ".*ERROR_AND_EXIT.*" \
    --exclude-noncode-lines \
    --exclude-function-lines \
    --exclude-unreachable-branches \
    --exclude-throw-branches \
    --merge-mode-functions=separate \
    --merge-mode-conditions=fold \
    --html --html-details -o build/coverage/gcovr/coverage.html \
    --lcov build/coverage/lcov/coverage_raw.info \

lcov --remove build/coverage/lcov/coverage_raw.info '/usr/* '\
     --demangle-cpp \
    --branch-coverage \
    --ignore-errors inconsistent,inconsistent \
    --ignore-errors mismatch,mismatch \
    --ignore-errors source,source \
    --ignore-errors unused \
    -o build/coverage/lcov/coverage.info

genhtml build/coverage/lcov/coverage.info \
    --output-directory build/coverage/lcov/ \
    --ignore-errors inconsistent,inconsistent \
    --ignore-errors mismatch,mismatch \
    --ignore-errors source,source \
    --ignore-errors unused \
    --no-checksum \
    --demangle-cpp \
    --branch-coverage \
    --function-coverage \
    --legend \
    --title "CommonLowLevelTracingKit" \
    --show-details

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
find build/coverage/ -name merged.profdata -delete
find build/coverage/ -name combined-coverage.info -delete

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
        llvm-profdata merge -sparse $dir/*.profraw \
        -o $dir/merged.profdata

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
    fi
    echo "$exe" >"$dir/name"
done

printf "%s\n" "${output[@]}" | column -t | sort
rm -rf build/profraw

echo "build/coverage/gcovr/coverage.html"
echo build/coverage/lcov/index.html