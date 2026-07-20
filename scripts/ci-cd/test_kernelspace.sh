#!/usr/bin/env bash
# Copyright (c) 2024, International Business Machines
# SPDX-License-Identifier: BSD-2-Clause-Patent

PERSITENT_ARTIFACTS="${PERSITENT_ARTIFACTS:-"./build_kernel/persistent"}"

# Note: version.gen.h is generated inside build_kernelspace.sh (build_modules ->
# generate_version_header) and by the kernel Makefile, both with explicit -t/-o
# arguments. No standalone pre-generation call is needed here.
./scripts/ci-cd/build_kernelspace.sh \
    -t "$PERSITENT_ARTIFACTS" \
    -k 5.10.52 \
    -b  \
    -s ./kernel_tracing_library/src \
    -s ./examples/simple_kernel_module \
    -m kernel_tracing_library \
    -m simple_tracing_test \
    -r
