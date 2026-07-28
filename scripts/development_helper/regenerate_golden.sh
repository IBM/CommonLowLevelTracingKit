#!/usr/bin/env bash
# Copyright (c) 2026, International Business Machines
# SPDX-License-Identifier: BSD-2-Clause-Patent

# Regenerate the deterministic golden fixture for the current HEAD version and
# write it into tests/golden/ with a version-named filename. Run this when the
# golden format gate reports that the trace format changed, then review and
# commit the new fixture. Old fixtures are kept -- they must stay decodable.
#
# Usage: regenerate_golden.sh [--arch <aarch64|s390x>] ...
#        (default: all supported architectures)

set -euo pipefail

ROOT_PATH=$(git rev-parse --show-toplevel)
cd "${ROOT_PATH}"

GEN_DIR="${ROOT_PATH}/tests/golden/generator"
# shellcheck source=tests/golden/generator/golden_lib.sh
source "${GEN_DIR}/golden_lib.sh"

GEN="${GEN_DIR}/generate.sh"
GOLDEN_DIR="${ROOT_PATH}/tests/golden"

ARCHES=()
FORCE=false
while [[ $# -gt 0 ]]; do
    case "$1" in
        --arch) ARCHES+=("$2"); shift 2 ;;
        --force) FORCE=true; shift ;;
        --help) echo "Usage: $0 [--arch <aarch64|s390x>] ... [--force]"; exit 0 ;;
        *) echo "Unknown option: $1" >&2; exit 1 ;;
    esac
done
[[ ${#ARCHES[@]} -gt 0 ]] || ARCHES=("${GOLDEN_ARCHES[@]}")

VERSION=$(grep -m1 -oE '^[0-9]+\.[0-9]+\.[0-9]+' VERSION.md)
[[ -n "${VERSION}" ]] || { echo "could not parse version from VERSION.md" >&2; exit 1; }
echo "Regenerating golden fixtures for version ${VERSION}"

TMP=$(mktemp -d)
trap 'rm -rf "${TMP}"' EXIT

written=()
for arch in "${ARCHES[@]}"; do
    suffix=$(golden_suffix_for "${arch}")
    dst="${GOLDEN_DIR}/${VERSION}/${suffix}.clltk_trace"
    if [ -e "${dst}" ] && [ "${FORCE}" != true ]; then
        echo ">>> ${arch}: skipping -- ${VERSION}/${suffix}.clltk_trace already exists"
        echo "    Old fixtures must stay decodable, so a format change needs a NEW version:"
        echo "    bump VERSION.md and re-run. Pass --force to replace this same-version"
        echo "    baseline (rarely correct). Missing arches at this version are still generated."
        continue
    fi
    echo ">>> ${arch}"
    "${GEN}" --src "${ROOT_PATH}" --out "${TMP}/${arch}" --arch "${arch}"
    mkdir -p "${GOLDEN_DIR}/${VERSION}"
    cp "${TMP}/${arch}/GOLDEN.clltk_trace" "${dst}"
    written+=("${dst}")
done

echo ""
if [ ${#written[@]} -eq 0 ]; then
    echo "Nothing written (all requested fixtures already exist)."
    exit 0
fi
echo "Wrote:"
for f in "${written[@]}"; do echo "  ${f#"${ROOT_PATH}"/}"; done
echo ""
echo "Review the new fixtures, add them to tests/test_golden.py's fixture lists"
echo "if the format changed, then commit them."
