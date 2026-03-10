#!/usr/bin/env bash
# Copyright (c) 2024, International Business Machines
# SPDX-License-Identifier: BSD-2-Clause-Patent

ROOT_PATH=$(git rev-parse --show-toplevel)

# ============================================
# Step 1: Check if version-sensitive files changed
# ============================================

# Patterns that require a version bump (library source, build system)
VERSION_REQUIRED_PATTERNS=(
	"tracing_library/"
	"decoder_tool/"
	"command_line_tool/"
	"snapshot_library/"
	"kernel_tracing_library/"
	"cmake/"
)

# Root-level files that require version bump
VERSION_REQUIRED_FILES=(
	"CMakeLists.txt"
	"CMakePresets.json"
)

# Determine base branch for comparison
if git rev-parse --verify origin/main >/dev/null 2>&1; then
	BASE_REF="origin/main"
elif git rev-parse --verify main >/dev/null 2>&1; then
	BASE_REF="main"
else
	echo "Could not find main branch - assuming version bump is required"
	BASE_REF=""
fi

if [ -n "$BASE_REF" ]; then
	# Get changed files between this branch and main
	CHANGED_FILES=$(git diff --name-only "$BASE_REF"...HEAD 2>/dev/null || git diff --name-only "$BASE_REF" HEAD)

	# Find which files trigger version requirement
	TRIGGERING_FILES=()
	for file in $CHANGED_FILES; do
		matched=false
		# Check directory patterns
		for pattern in "${VERSION_REQUIRED_PATTERNS[@]}"; do
			if [[ "$file" == "$pattern"* ]]; then
				TRIGGERING_FILES+=("$file")
				matched=true
				break
			fi
		done
		# Check exact file matches (root-level files) if not already matched
		if [ "$matched" = false ]; then
			for exact in "${VERSION_REQUIRED_FILES[@]}"; do
				if [[ "$file" == "$exact" ]]; then
					TRIGGERING_FILES+=("$file")
					break
				fi
			done
		fi
	done

	if [ ${#TRIGGERING_FILES[@]} -eq 0 ]; then
		echo "Only non-library files changed (tests, docs, scripts, etc.)"
		echo "No version bump required for this PR."
		exit 0
	fi

	echo "Library or build system files changed:"
	for file in "${TRIGGERING_FILES[@]}"; do
		echo "  - $file"
	done
	echo ""
	echo "Checking version bump..."
fi

# ============================================
# Step 2: Version comparison
# ============================================

THIS_MAJOR=$(git show HEAD:VERSION.md | head -n 1 | cut -d . -f 1)
THIS_MINOR=$(git show HEAD:VERSION.md | head -n 1 | cut -d . -f 2)
THIS_PATCH=$(git show HEAD:VERSION.md | head -n 1 | cut -d . -f 3)

THIS_VERSION=$(((THIS_MAJOR * 0x10000) + (THIS_MINOR * 0x100) + (THIS_PATCH * 0x1)))

MAIN_VERSION=$(git show origin/main:VERSION.md 2>/dev/null)
if [ $? -ne 0 ]; then
	MAIN_VERSION=$(git show main:VERSION.md)
fi

if [[ $MAIN_VERSION =~ ([0-9]+\.[0-9]+\.[0-9]+) ]]; then
	MAIN_VERSION="${BASH_REMATCH[1]}"
else
	echo "No version number in main"
	exit 0
fi

MAIN_MAJOR=$(echo $MAIN_VERSION | cut -d . -f 1)
MAIN_MINOR=$(echo $MAIN_VERSION | cut -d . -f 2)
MAIN_PATCH=$(echo $MAIN_VERSION | cut -d . -f 3)

MAIN_VERSION_NUM=$(((MAIN_MAJOR * 0x10000) + (MAIN_MINOR * 0x100) + (MAIN_PATCH * 0x1)))

echo "PR version:   $THIS_MAJOR.$THIS_MINOR.$THIS_PATCH"
echo "Main version: $MAIN_MAJOR.$MAIN_MINOR.$MAIN_PATCH"

if [ $THIS_VERSION -gt $MAIN_VERSION_NUM ]; then
	echo "Version bumped - good to go!"
	exit 0
else
	echo ""
	echo "Please bump the version in VERSION.md before merging."
	echo "Current main is $MAIN_MAJOR.$MAIN_MINOR.$MAIN_PATCH - update to at least $MAIN_MAJOR.$MAIN_MINOR.$((MAIN_PATCH + 1))"
	exit 1
fi
