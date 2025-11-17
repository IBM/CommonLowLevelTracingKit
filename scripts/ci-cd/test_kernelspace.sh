#!/usr/bin/env bash
# Copyright (c) 2024, International Business Machines
# SPDX-License-Identifier: BSD-2-Clause-Patent

PERSITENT_ARTIFACTS="${PERSITENT_ARTIFACTS:-"./build_kernel/persistent"}"

./cmake/gen_version_header.sh
./scripts/ci-cd/build_kernelspace.sh \
    -t "$PERSITENT_ARTIFACTS" \
    -k 5.10.52 \
    -b  \
    -s ./kernel_tracing_library/src \
    -s ./examples/simple_kernel_module \
    -m kernel_tracing_library \
    -m simple_tracing_test \
    -r
