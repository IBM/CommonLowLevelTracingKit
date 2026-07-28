#!/usr/bin/env bash
# Copyright (c) 2026, International Business Machines
# SPDX-License-Identifier: BSD-2-Clause-Patent

# In-container golden fixture generator. Configures the source tree at /src with
# the `golden` cmake preset (frozen identity, tracing lib only), builds the
# golden-generate target (which runs the writer into <build>/golden/), and
# copies the resulting fixtures to /out. Driven from the host by generate.sh.
#
# --preset supplies every cache variable; -B redirects the build out of the
# read-only /src mount.
set -e
dnf install -y -q gcc gcc-c++ cmake make git gettext-envsubst rsync > /dev/null 2>&1
git config --global --add safe.directory "*"

cd /src
cmake --preset golden -B /tmp/b > /tmp/cfg.log 2>&1 || { tail -20 /tmp/cfg.log; exit 1; }
cmake --build /tmp/b --target golden-generate -- -j4 > /tmp/build.log 2>&1 || { tail -30 /tmp/build.log; exit 1; }
cp /tmp/b/golden/GOLDEN.clltk_trace /out/

# ELF metadata objects only on request (CLLTK_GOLDEN_ELF=1); the format gate
# does not need them, so it never builds them.
if [ "${CLLTK_GOLDEN_ELF:-0}" = "1" ]; then
	cmake --build /tmp/b --target golden-elf -- -j4 >> /tmp/build.log 2>&1 || { tail -30 /tmp/build.log; exit 1; }
	cp /tmp/b/golden/elfm.o /tmp/b/golden/elfm.so /out/
fi

# the writer creates 0640 files; under a rootful engine (docker in CI) they are
# owned by root and the host caller cannot read them. Make them world-readable.
chmod 0644 /out/* 2>/dev/null || true

echo "fixtures written:"
ls -l /out/
