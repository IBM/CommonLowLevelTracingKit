#!/usr/bin/env bash
# Copyright (c) 2024, International Business Machines
# SPDX-License-Identifier: BSD-2-Clause-Patent

# CI Step: Build the kernel modules against the distro kernel headers.
#
# This is a compile-and-link check (including modpost symbol resolution),
# not a runtime test - for that see test_kernelspace.sh, which boots a
# QEMU kernel. Linking matters: the module can compile per-object but
# fail modpost, e.g. when gcc emits libgcc calls that do not exist in
# kernel space (outline atomics on aarch64).
#
# Exit: 0 = success, 1 = build failed

set -euo pipefail

ROOT_PATH=$(git rev-parse --show-toplevel)
cd "${ROOT_PATH}"

echo "========================================"
echo "CI Step: Build Kernel Modules"
echo "========================================"

if ! ls /usr/src/kernels/*/Makefile >/dev/null 2>&1; then
    echo "Installing kernel-devel..."
    # The CI container caches dnf metadata at image build time (refreshed weekly).
    # By mid-week that metadata can still reference a kernel-devel NEVRA that
    # Fedora has already superseded and pruned from its mirrors, so a plain
    # install 404s on every mirror. --refresh forces fresh metadata; the retry
    # rides out transient mirror interruptions.
    PKGS=(kernel-devel flex bison elfutils-libelf-devel openssl-devel bc)
    for attempt in 1 2 3; do
        if dnf -y --refresh install "${PKGS[@]}" >/dev/null; then
            break
        fi
        if [ "$attempt" -eq 3 ]; then
            echo "package install failed after $attempt attempts" >&2
            exit 1
        fi
        echo "dnf install failed (attempt $attempt), clearing cache and retrying..." >&2
        dnf -y clean expire-cache >/dev/null 2>&1 || true
        sleep 15
    done
fi
KDIR=$(ls -d /usr/src/kernels/* | head -1)
echo "Kernel tree: $KDIR"

echo ""
echo "Building clltk_kernel_tracing..."
# cd instead of make -C: the module Makefile derives repo paths from $(PWD).
# It generates version.gen.h itself.
if ! (cd kernel_tracing_library/src && make KERNEL_SRC="$KDIR" modules); then
    echo "FAILED: kernel tracing module build failed"
    exit 1
fi

echo ""
echo "Building simple_kernel_module example..."
if ! make -C "$KDIR" M="$ROOT_PATH/examples/simple_kernel_module" \
        KBUILD_EXTRA_SYMBOLS="$ROOT_PATH/kernel_tracing_library/src/Module.symvers" modules; then
    echo "FAILED: example kernel module build failed"
    exit 1
fi

echo ""
echo "PASSED: kernel modules build and link"
exit 0
