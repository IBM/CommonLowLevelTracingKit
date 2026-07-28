#!/usr/bin/env bash
# Copyright (c) 2026, International Business Machines
# SPDX-License-Identifier: BSD-2-Clause-Patent

# Host-side driver for deterministic golden fixture generation.
#
# Runs the in-container generator (gen.sh) against a source tree and a target
# architecture, writing GOLDEN.clltk_trace into the output directory. This is
# the single place the
# container / mount / arch contract lives:
#
#   /src  = source tree to build the tracing library from
#   /out  = destination for the generated fixtures
#
# gen.sh builds the golden-generate cmake target (CLLTK_GOLDEN option), whose
# writer links frozen_info.c for byte-deterministic output. With --elf it also
# builds the golden-elf target and emits elfm.o/elfm.so (the ELF metadata
# objects); the format gate omits it so an ELF build issue can't trip the gate.
# s390x (big endian) runs under qemu-user-static; register binfmt once with:
#   podman run --rm --privileged docker.io/tonistiigi/binfmt --install s390x,arm64
#
# Usage: generate.sh --src <dir> --out <dir> --arch <aarch64|s390x> [--elf]

set -euo pipefail

CONTAINER_CMD=${CLLTK_CONTAINER_CMD:-podman}
IMAGE=${CLLTK_GOLDEN_IMAGE:-registry.fedoraproject.org/fedora:43}

SRC="" OUT="" ARCH="" WITH_ELF=0

usage() {
	echo "Usage: $0 --src <dir> --out <dir> --arch <aarch64|s390x> [--elf]"
	echo ""
	echo "Generate deterministic golden trace fixtures from a source tree."
	echo ""
	echo "  --src <dir>    source tree to build the tracing library from"
	echo "  --out <dir>    destination directory for the fixtures"
	echo "  --arch <arch>  target architecture: aarch64 (LE) or s390x (BE)"
	echo "  --elf          also emit elfm.o/elfm.so (ELF metadata objects)"
	echo ""
	echo "Environment:"
	echo "  CLLTK_CONTAINER_CMD   container engine (default: podman)"
	echo "  CLLTK_GOLDEN_IMAGE    builder image (default: fedora:43)"
	exit "${1:-0}"
}

while [[ $# -gt 0 ]]; do
	case "$1" in
		--src) SRC="$2"; shift 2 ;;
		--out) OUT="$2"; shift 2 ;;
		--arch) ARCH="$2"; shift 2 ;;
		--elf) WITH_ELF=1; shift ;;
		--help) usage 0 ;;
		*) echo "Unknown option: $1" >&2; usage 1 ;;
	esac
done

[[ -n "$SRC" && -n "$OUT" && -n "$ARCH" ]] || { echo "missing --src/--out/--arch" >&2; usage 1; }
[[ -d "$SRC" ]] || { echo "source tree not found: $SRC" >&2; exit 1; }

case "$ARCH" in
	aarch64) PLATFORM=linux/arm64 ;;
	s390x)   PLATFORM=linux/s390x ;;
	*) echo "unsupported arch: $ARCH (expected aarch64 or s390x)" >&2; exit 1 ;;
esac

SRC="$(cd "$SRC" && pwd)"
mkdir -p "$OUT"
OUT="$(cd "$OUT" && pwd)"

"$CONTAINER_CMD" run --rm --platform "$PLATFORM" \
	-e "CLLTK_GOLDEN_ELF=$WITH_ELF" \
	-v "$SRC:/src:ro" \
	-v "$OUT:/out:z" \
	"$IMAGE" bash /src/tests/golden/generator/gen.sh
