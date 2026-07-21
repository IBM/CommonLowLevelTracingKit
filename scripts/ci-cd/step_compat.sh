#!/usr/bin/env bash
# Copyright (c) 2024, International Business Machines
# SPDX-License-Identifier: BSD-2-Clause-Patent

# CI Step: compiler-compatibility smoke test.
#
# Builds ONLY the tracing library plus the minimal simple_c / simple_cpp
# examples with the selected compiler, runs them, and confirms each produces a
# trace file. This is the cheap gate that a consumer can compile, link, and run
# a tracepoint on a given gcc/clang version - not the full test suite. The
# library is dependency-light, so the heavy components (CLI tool, decoders,
# snapshot, kernel module, tests) are turned off and no extra libraries are
# needed beyond a C/C++ compiler and CMake.
#
# The library builds with -Werror (COMPILE_WARNING_AS_ERROR), which is kept on
# purpose: a new compiler's warnings should surface here.
#
# Runnable locally, e.g.:  CC=gcc-13 CXX=g++-13 ./scripts/ci-cd/step_compat.sh

set -euo pipefail
ROOT_PATH=$(git rev-parse --show-toplevel 2>/dev/null || pwd)
cd "${ROOT_PATH}"

CC="${CC:-cc}"
CXX="${CXX:-c++}"
BUILD_DIR="${BUILD_DIR:-build/compat}"

echo "========================================"
echo "CI Step: Compiler compatibility"
echo "  CC =$CC : $("$CC" --version 2>/dev/null | head -1)"
echo "  CXX=$CXX : $("$CXX" --version 2>/dev/null | head -1)"
echo "========================================"

# Library + examples only; heavy components off so no external deps are needed.
cmake -S . -B "${BUILD_DIR}" \
    -DCMAKE_C_COMPILER="${CC}" \
    -DCMAKE_CXX_COMPILER="${CXX}" \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DCLLTK_TRACING=ON \
    -DCLLTK_EXAMPLES=ON \
    -DCLLTK_SNAPSHOT=OFF \
    -DCLLTK_COMMAND_LINE_TOOL=OFF \
    -DCLLTK_CPP_DECODER=OFF \
    -DCLLTK_PYTHON_DECODER=OFF \
    -DCLLTK_KERNEL_TRACING=OFF \
    -DCLLTK_TESTS=OFF

cmake --build "${BUILD_DIR}" \
    --target example-simple_c example-simple_cpp \
    -j"$(nproc)"

C_BIN=$(find "${BUILD_DIR}" -type f -name example-simple_c   | head -1)
CPP_BIN=$(find "${BUILD_DIR}" -type f -name example-simple_cpp | head -1)
[ -x "${C_BIN}" ]   || { echo "FAILED: example-simple_c not built";   exit 1; }
[ -x "${CPP_BIN}" ] || { echo "FAILED: example-simple_cpp not built"; exit 1; }

OUT=$(mktemp -d)
export CLLTK_TRACING_PATH="${OUT}"
echo "Running examples (CLLTK_TRACING_PATH=${OUT})..."
"${C_BIN}"
"${CPP_BIN}"

echo "Trace files produced:"
ls -l "${OUT}"
if [ -z "$(find "${OUT}" -name '*.clltk_trace' -print -quit)" ]; then
    echo "FAILED: examples ran but produced no .clltk_trace file"
    exit 1
fi

# ---------------------------------------------------------------------------
# Text-relocation guard for the "tracepoint inside a shared library" shape.
#
# A consumer shared object (a plugin/driver .so) expands the tracepoint macros
# *inside the .so*, which emits the _metaptr metadata section. If that section
# is ever read-only, its absolute pointers become text relocations
# (DT_TEXTREL): -fPIC cannot avoid them, hardened targets (AArch64) reject them
# at load time, and Yocto's do_package_qa fails the build. The main test suite
# links such a shared library but only ever *warns* on TEXTREL, so the
# regression stayed invisible. Here we build a minimal tracepoint-bearing
# shared object with "-z text", which makes the linker refuse to emit any text
# relocation - a hard error. Because this runs on every compiler and every arch
# in the matrix (incl. arm64 and s390x under QEMU), it catches both generic and
# arch-specific reintroductions. A shared object may keep undefined symbols, so
# no CLLTK runtime needs to be linked - the relocations are a property of the
# consumer object alone.
echo "Checking tracepoint-in-shared-library is text-relocation free (-z text)..."
GUARD_SRC="${OUT}/clltk_textrel_guard.c"
GUARD_SO="${OUT}/libclltk_textrel_guard.so"
cat > "${GUARD_SRC}" <<'GUARD_EOF'
#include "CommonLowLevelTracingKit/tracing/tracing.h"
CLLTK_TRACEBUFFER(compat_textrel_guard, 4096);
void clltk_compat_textrel_guard(int x) {
    CLLTK_TRACEPOINT(compat_textrel_guard, "textrel guard %d", x);
}
GUARD_EOF
if ! "${CC}" -fPIC -shared -O2 -I"${ROOT_PATH}/tracing_library/include" \
        -Wl,-z,text "${GUARD_SRC}" -o "${GUARD_SO}"; then
    echo "FAILED: a tracepoint compiled into a shared library produced text"
    echo "        relocations (DT_TEXTREL). This breaks PIE/hardened targets"
    echo "        and Yocto do_package_qa. See _CLLTK_EMIT_META_PTR - the"
    echo "        _metaptr section must be emitted writable (\"aw\")."
    exit 1
fi
# Belt and suspenders: fail if a TEXTREL somehow survived the link (e.g. a
# linker that only warns). Skipped if readelf is unavailable.
if command -v readelf >/dev/null 2>&1; then
    if readelf -dW "${GUARD_SO}" 2>/dev/null | grep -qiE '\bTEXTREL\b'; then
        echo "FAILED: DT_TEXTREL present in ${GUARD_SO} despite -z text"
        exit 1
    fi
fi

echo "PASSED: library + minimal examples build, run, and trace with ${CC}/${CXX}"
echo "PASSED: tracepoint-in-shared-library is free of text relocations"
