# Copyright (c) 2024, International Business Machines
# SPDX-License-Identifier: BSD-2-Clause-Patent

set -e

PROJECT_ROOT="$(git rev-parse --show-toplevel)"


if [ -z $SOURCE_PATH ]; then
    export SOURCE_PATH="$PROJECT_ROOT"
    mkdir -p "$SOURCE_PATH"
fi

if [ -z $BUILD_PATH ]; then
    export BUILD_PATH="$SOURCE_PATH/build"
    mkdir -p "$BUILD_PATH"
fi

if [ -z $BUILD_CACHE ]; then
    export BUILD_CACHE="$SOURCE_PATH/build_kernel"
    mkdir -p "$BUILD_CACHE"
fi

if [ -z $INITRD_ROOT ]; then
    export INITRD_ROOT="$BUILD_CACHE/rootfs"
fi

source $SOURCE_PATH/scripts/ci-cd/kernel/kernel_102_run_test_debug.sh
