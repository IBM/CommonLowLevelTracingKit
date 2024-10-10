#!/usr/bin/env bash
# Copyright (c) 2024, International Business Machines
# SPDX-License-Identifier: BSD-2-Clause-Patent

ROOT_PATH=$(git rev-parse --show-toplevel)

THIS_MAJOR=$(git show HEAD:VERSION.md | head -n 1 | cut -d . -f 1 )
THIS_MINOR=$(git show HEAD:VERSION.md | head -n 1 | cut -d . -f 2 )
THIS_PATCH=$(git show HEAD:VERSION.md | head -n 1 | cut -d . -f 3 )

THIS_VERSION=$(((THIS_MAJOR * 0x10000) + (THIS_MINOR * 0x100) + (THIS_PATCH * 0x1)))


MAIN_VERSION=$(git show origin/main:VERSION.md)
if [ $? -ne 0 ]; then
	MAIN_VERSION=$(git show main:VERSION.md)
fi

if [[ $MAIN_VERSION =~ ([0-9]+\.[0-9]+\.[0-9]+) ]]; then
  MAIN_VERSION="${BASH_REMATCH[1]}"
else
  echo "No version number in main"
  exit
fi

MAIN_MAJOR=$(echo $MAIN_VERSION | cut -d . -f 1 )
MAIN_MINOR=$(echo $MAIN_VERSION | cut -d . -f 2 )
MAIN_PATCH=$(echo $MAIN_VERSION | cut -d . -f 3 )


MAIN_VERSION=$(((MAIN_MAJOR * 0x10000) + (MAIN_MINOR * 0x100) + (MAIN_PATCH * 0x1)))

echo "THIS:$THIS_VERSION MAIN:$MAIN_VERSION"


if [ $THIS_VERSION -gt $MAIN_VERSION ]; then
	echo "Version stepped"
	exit 0
else
	echo "Version not stepped"
	exit 1
fi

