#!/usr/bin/env bash
# Copyright (c) 2024, International Business Machines
# SPDX-License-Identifier: BSD-2-Clause-Patent

SRC_TOPLEVEL="$(dirname $0)/.."
TEMPLATE="$SRC_TOPLEVEL/cmake/version.h.template"
OUTPUT="$SRC_TOPLEVEL/tracing_library/include/CommonLowLevelTracingKit/version.gen.h"

print_help() {
echo "$0 [-t TEMPLATE_FILE] [-o OUTPUT_FILE]"
echo "\
    args:
        -t | --template TEMPLAE_FILE (default $TEMPLATE)
        -o | --output   OUTPUT_FILE  (default $OUTPUT)
    
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

CLLTK_VERSION=$(cat "$SRC_TOPLEVEL/VERSION.md" | head -n1 )
CLLTK_VERSION_MAJOR=$(echo $CLLTK_VERSION | cut -d . -f 1 )
CLLTK_VERSION_MINOR=$(echo $CLLTK_VERSION | cut -d . -f 2 )
CLLTK_VERSION_PATCH=$(echo $CLLTK_VERSION | cut -d . -f 3 )

temp_file=$(mktemp)
env CLLTK_VERSION_MAJOR=$CLLTK_VERSION_MAJOR \
    CLLTK_VERSION_MINOR=$CLLTK_VERSION_MINOR \
    CLLTK_VERSION_PATCH=$CLLTK_VERSION_PATCH \
    envsubst < "$TEMPLATE" > "$temp_file"

rsync --checksum $temp_file $OUTPUT
rm -f "$temp_file"
