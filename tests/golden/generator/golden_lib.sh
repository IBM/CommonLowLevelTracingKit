# Copyright (c) 2026, International Business Machines
# SPDX-License-Identifier: BSD-2-Clause-Patent
# shellcheck shell=bash

# Shared definitions for the golden fixture scripts (sourced, not executed):
# the supported architectures and each one's committed-fixture suffix. Single
# source of truth so the gate and the regenerate helper cannot drift.

# shellcheck disable=SC2034  # consumed by sourcing scripts
GOLDEN_ARCHES=(aarch64 s390x)

golden_suffix_for() {
	case "$1" in
		aarch64) echo "le-aarch64" ;;
		s390x)   echo "be-s390x" ;;
		*) echo "unsupported arch: $1" >&2; return 1 ;;
	esac
}
