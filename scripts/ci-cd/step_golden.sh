#!/usr/bin/env bash
# Copyright (c) 2026, International Business Machines
# SPDX-License-Identifier: BSD-2-Clause-Patent

# CI Step: Golden format gate.
#
# Regenerates the deterministic golden fixture at HEAD for each architecture and
# byte-compares it (with the library-version field masked) against the newest
# committed golden of the same architecture. A difference means the trace-file
# format changed and a new fixture must be committed; an identical result means
# the existing corpus already covers this format.
#
# Generation is byte-deterministic (frozen_info.c), so the comparison is a plain
# cmp -- no absolute expected output to maintain. Correctness of the fixtures
# themselves (both decoders agree) is checked separately by tests/test_golden.py
# under step_test.sh.
#
# Usage: ./scripts/ci-cd/step_golden.sh
# Env:   CLLTK_GOLDEN_ARCHES   space-separated arches to check (default: all)
# Exit:  0 = corpus current, 1 = format changed / non-deterministic / missing baseline

set -euo pipefail

ROOT_PATH=$(git rev-parse --show-toplevel)
cd "${ROOT_PATH}"

GEN_DIR="${ROOT_PATH}/tests/golden/generator"
# shellcheck source=tests/golden/generator/golden_lib.sh
source "${GEN_DIR}/golden_lib.sh"

GEN="${GEN_DIR}/generate.sh"
NORMALIZE="${GEN_DIR}/normalize.py"
GOLDEN_DIR="${ROOT_PATH}/tests/golden"
ARCHES=${CLLTK_GOLDEN_ARCHES:-${GOLDEN_ARCHES[*]}}

echo "========================================"
echo "CI Step: Golden Format Gate"
echo "========================================"

TMP=$(mktemp -d)
trap 'rm -rf "${TMP}"' EXIT
fail=0

# newest committed fixture for a given arch suffix, across version folders
# (tests/golden/<version>/<suffix>.clltk_trace)
newest_golden() {
    local suffix="$1" d f ver
    local entries=()
    for d in "${GOLDEN_DIR}"/*/; do
        f="${d}${suffix}.clltk_trace"
        [ -e "${f}" ] || continue
        ver=$(basename "${d%/}")
        entries+=("${ver}"$'\t'"${f}")
    done
    [ ${#entries[@]} -eq 0 ] && return 0
    printf '%s\n' "${entries[@]}" | sort -V | tail -1 | cut -f2-
}

for arch in ${ARCHES}; do
    suffix=$(golden_suffix_for "${arch}")
    echo ""
    echo ">>> ${arch}: generating candidate (twice, for determinism)"
    if ! "${GEN}" --src "${ROOT_PATH}" --out "${TMP}/${arch}.1" --arch "${arch}" >/dev/null \
       || ! "${GEN}" --src "${ROOT_PATH}" --out "${TMP}/${arch}.2" --arch "${arch}" >/dev/null; then
        echo "  FAIL: generation failed for ${arch} (is a container engine with qemu available?)"
        fail=1
        continue
    fi

    c1="${TMP}/${arch}.1/GOLDEN.clltk_trace"
    c2="${TMP}/${arch}.2/GOLDEN.clltk_trace"

    # determinism self-check on normalized bytes: frozen identity makes two runs
    # identical apart from the masked ephemeral fields (library version + the
    # section mutexes). A real unfrozen identity field (timestamp/pid/tid/span)
    # is not masked, so it still fails here.
    python3 "${NORMALIZE}" "${c1}" "${TMP}/d1.norm"
    python3 "${NORMALIZE}" "${c2}" "${TMP}/d2.norm"
    if ! cmp -s "${TMP}/d1.norm" "${TMP}/d2.norm"; then
        echo "  FAIL: generation is non-deterministic (an identity field is not frozen)"
        echo "        differing offsets (normalized, byte octal-values):"
        cmp -l "${TMP}/d1.norm" "${TMP}/d2.norm" 2>/dev/null | head -20 | sed 's/^/          /'
        fail=1
        continue
    fi

    committed=$(newest_golden "${suffix}")
    if [ -z "${committed}" ]; then
        echo "  FAIL: no committed golden for ${suffix} -- commit a baseline fixture"
        fail=1
        continue
    fi

    python3 "${NORMALIZE}" "${c1}" "${TMP}/cand.norm"
    python3 "${NORMALIZE}" "${committed}" "${TMP}/base.norm"
    committed_rel="${committed#"${GOLDEN_DIR}/"}"
    if cmp -s "${TMP}/cand.norm" "${TMP}/base.norm"; then
        echo "  OK: matches ${committed_rel}"
    else
        echo "  FAIL: format changed vs ${committed_rel}"
        echo "        the trace format changed; regenerate and commit a new fixture:"
        echo "          ./scripts/development_helper/regenerate_golden.sh --arch ${arch}"
        echo "        (see tests/golden/README.md)"
        fail=1
    fi
done

echo ""
if [ "${fail}" -eq 0 ]; then
    echo "========================================"
    echo "PASSED: golden corpus is current"
    echo "========================================"
    exit 0
else
    echo "========================================"
    echo "FAILED: golden format gate"
    echo "========================================"
    exit 1
fi
