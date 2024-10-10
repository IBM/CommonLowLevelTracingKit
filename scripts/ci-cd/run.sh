#! /bin/bash
# Copyright (c) 2024, International Business Machines
# SPDX-License-Identifier: BSD-2-Clause-Patent

ROOT_PATH=${ROOT_PATH:=$(git rev-parse --show-toplevel)}
cd $ROOT_PATH

# test version step
version_failed=0
if [ "${IF_PULL_REQUEST}" != "false" ]; then
    scripts/ci-cd/check_version_step.sh
    version_failed=$?
    if [ $version_failed -eq 0 ]; then
        echo "version step valid"
    else
        echo "version step missing"
    fi
else
    echo "only check if it is a pull request"
fi

# clean build dir
BUILD_DIR=${BUILD_DIR:-./build/}
rm -rf $BUILD_DIR/*


# test formation
echo "format check start"
scripts/ci-cd/check_format.sh
format_failed=$?
if [ $format_failed -eq 0 ]; then
    echo "format check valid"
else
    echo "format check failed"
fi
echo

echo "userspace build start"
scripts/userspace/build.sh
build_status=$?
if [ $build_status -eq 0 ]; then
    echo "userspace build success"
else
    echo "userspace build failed"
    exit 1
fi
echo

echo "userspace test start"
scripts/userspace/test.sh
test_status=$?
if [ $test_status -eq 0 ]; then
    echo "userspace test success"
else
    echo "userspace test failed"
    exit 1
fi
echo

examples=(
    example-simple_c
    example-complex_c
    example-extreme_c
    example-gen_format_c
    example-simple_cpp
    example-complex_cpp
    example-with_libraries
    example-process_threads
)
memory_check_failed=0
for example in "${examples[@]}"; do
    scripts/userspace/run_with_memcheck.sh $example
    current_memory_check_failed=$?
    memory_check_failed+=$current_memory_check_failed
    if [ $current_memory_check_failed -eq 0 ]; then
        echo "memory check for $example valid"
    else
        echo "memory check for $example failed"
        exit 1
    fi
    echo
done

if [ $version_failed -ne 0 ]; then
    if [ "${CLLTK_CI_VERSION_STEP_REQUIRED:-true}" == "false" ]; then
        echo "version step missing but not required"
        version_failed=0
    else
        echo "version step missing"
    fi 
fi
if [ $format_failed -ne 0 ]; then
    echo "format check failed"
else
    echo "format check passed"
fi
if [ $version_failed -ne 0 ] || [ $format_failed -ne 0 ]; then
    echo "ci failed"
    exit 1
else
    echo "ci passed"
    exit 0
fi

echo ci done
