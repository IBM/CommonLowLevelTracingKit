#!/usr/bin/env bash
# Copyright (c) 2024, International Business Machines
# SPDX-License-Identifier: BSD-2-Clause-Patent

ROOT_PATH=$(git rev-parse --show-toplevel)
echo "ROOT_PATH=${ROOT_PATH}"
cd ${ROOT_PATH}

TRACEBUFFERS=$(find ${ROOT_PATH}/build/ -not \( -path ${ROOT_PATH}/build/tests -prune \)  -type f -name "*.clltk_trace" ) 
echo "TRACEBUFFERS=${TRACEBUFFERS}"

DECODER="${ROOT_PATH}/decoder_tool/python/clltk_decoder.py"

${DECODER} -o ${ROOT_PATH}/build/tracebuffers.csv ${TRACEBUFFERS}
