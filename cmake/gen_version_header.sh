#!/usr/bin/env bash
# Copyright (c) 2024, International Business Machines
# SPDX-License-Identifier: BSD-2-Clause-Patent

set -e

SRC_TOPLEVEL="$(dirname "$0")/.."

# Version header generation (optional if only generating build_info)
TEMPLATE=""
OUTPUT=""

# Build info generation (optional, for command line tool)
BUILD_INFO_TEMPLATE=""
BUILD_INFO_OUTPUT=""

# Feature flags (default to 0/unknown when not specified)
CLLTK_HAS_TRACING="${CLLTK_HAS_TRACING:-0}"
CLLTK_HAS_SNAPSHOT="${CLLTK_HAS_SNAPSHOT:-0}"
CLLTK_HAS_CPP_DECODER="${CLLTK_HAS_CPP_DECODER:-0}"
CLLTK_HAS_PYTHON_DECODER="${CLLTK_HAS_PYTHON_DECODER:-0}"
CLLTK_HAS_KERNEL_TRACING="${CLLTK_HAS_KERNEL_TRACING:-0}"

print_help() {
echo "$0 [-t TEMPLATE_FILE] [-o OUTPUT_FILE] [-b BUILD_INFO_TEMPLATE] [-B BUILD_INFO_OUTPUT]"
echo "\
    Generate version.gen.h and/or build_info.gen.h from templates.
    At least one of (-t and -o) or (-b and -B) must be specified.

    args:
        -t | --template     TEMPLATE_FILE       version.h template
        -o | --output       OUTPUT_FILE         version.h output
        -b | --build-info-template TEMPLATE     build_info.h template
        -B | --build-info-output   OUTPUT       build_info.h output
    
    environment variables for build_info (feature flags):
        CLLTK_HAS_TRACING        (0 or 1, default 0)
        CLLTK_HAS_SNAPSHOT       (0 or 1, default 0)
        CLLTK_HAS_CPP_DECODER    (0 or 1, default 0)
        CLLTK_HAS_PYTHON_DECODER (0 or 1, default 0)
        CLLTK_HAS_KERNEL_TRACING (0 or 1, default 0)
    
    other:
        -h | --help this help
    "
}

while (($#)) ; do 
    case $1 in
        -t | --template)
            TEMPLATE="$2"
            shift 2
        ;;
        -o | --output)
            OUTPUT="$2"
            shift 2
        ;;
        -b | --build-info-template)
            BUILD_INFO_TEMPLATE="$2"
            shift 2
        ;;
        -B | --build-info-output)
            BUILD_INFO_OUTPUT="$2"
            shift 2
        ;;
        -h | --help)
            print_help
            exit
        ;;
        *)
            echo "unknown argument \"$1\""
            print_help
            exit 1
        ;;
    esac
done

# Validate that at least one output is requested
if [[ -z "$TEMPLATE" && -z "$BUILD_INFO_TEMPLATE" ]]; then
    echo "Error: At least one of -t/-o or -b/-B must be specified"
    print_help
    exit 1
fi

# Parse version from VERSION.md
CLLTK_VERSION=$(head -n1 "$SRC_TOPLEVEL/VERSION.md")
CLLTK_VERSION_MAJOR=$(echo "$CLLTK_VERSION" | cut -d . -f 1)
CLLTK_VERSION_MINOR=$(echo "$CLLTK_VERSION" | cut -d . -f 2)
CLLTK_VERSION_PATCH=$(echo "$CLLTK_VERSION" | cut -d . -f 3)

# Get git information
if git -C "$SRC_TOPLEVEL" rev-parse --git-dir > /dev/null 2>&1; then
    CLLTK_GIT_HASH=$(git -C "$SRC_TOPLEVEL" rev-parse --short HEAD 2>/dev/null || echo "unknown")
    if git -C "$SRC_TOPLEVEL" diff --quiet 2>/dev/null; then
        CLLTK_GIT_DIRTY=0
    else
        CLLTK_GIT_DIRTY=1
    fi
else
    CLLTK_GIT_HASH="unknown"
    CLLTK_GIT_DIRTY=0
fi

# Generate version.gen.h (if templates provided)
if [[ -n "$TEMPLATE" && -n "$OUTPUT" ]]; then
    temp_file=$(mktemp)
    env CLLTK_VERSION_MAJOR="$CLLTK_VERSION_MAJOR" \
        CLLTK_VERSION_MINOR="$CLLTK_VERSION_MINOR" \
        CLLTK_VERSION_PATCH="$CLLTK_VERSION_PATCH" \
        envsubst < "$TEMPLATE" > "$temp_file"

    rsync --checksum "$temp_file" "$OUTPUT"
    rm -f "$temp_file"
fi

# Generate build_info.gen.h (if templates provided)
if [[ -n "$BUILD_INFO_TEMPLATE" && -n "$BUILD_INFO_OUTPUT" ]]; then
    temp_file=$(mktemp)
    env CLLTK_VERSION_MAJOR="$CLLTK_VERSION_MAJOR" \
        CLLTK_VERSION_MINOR="$CLLTK_VERSION_MINOR" \
        CLLTK_VERSION_PATCH="$CLLTK_VERSION_PATCH" \
        CLLTK_GIT_HASH="$CLLTK_GIT_HASH" \
        CLLTK_GIT_DIRTY="$CLLTK_GIT_DIRTY" \
        CLLTK_HAS_TRACING="$CLLTK_HAS_TRACING" \
        CLLTK_HAS_SNAPSHOT="$CLLTK_HAS_SNAPSHOT" \
        CLLTK_HAS_CPP_DECODER="$CLLTK_HAS_CPP_DECODER" \
        CLLTK_HAS_PYTHON_DECODER="$CLLTK_HAS_PYTHON_DECODER" \
        CLLTK_HAS_KERNEL_TRACING="$CLLTK_HAS_KERNEL_TRACING" \
        envsubst < "$BUILD_INFO_TEMPLATE" > "$temp_file"
    
    rsync --checksum "$temp_file" "$BUILD_INFO_OUTPUT"
    rm -f "$temp_file"
fi
