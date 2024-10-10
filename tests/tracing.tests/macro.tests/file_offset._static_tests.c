// Copyright (c) 2024, International Business Machines
// SPDX-License-Identifier: BSD-2-Clause-Patent

#include "CommonLowLevelTracingKit/tracing.h"
#include <stdbool.h>
#include <stdint.h>

_CLLTK_STATIC_ASSERT(false == _CLLTK_FILE_OFFSET_IS_STATIC(_clltk_file_offset_unset),
					 "_CLLTK_FILE_OFFSET_IS_STATIC failed");
_CLLTK_STATIC_ASSERT(false == _CLLTK_FILE_OFFSET_IS_STATIC(_clltk_file_offset_dynamic),
					 "_CLLTK_FILE_OFFSET_IS_STATIC failed");
_CLLTK_STATIC_ASSERT(false == _CLLTK_FILE_OFFSET_IS_STATIC(_clltk_file_offset_invalid),
					 "_CLLTK_FILE_OFFSET_IS_STATIC failed");
_CLLTK_STATIC_ASSERT(false == _CLLTK_FILE_OFFSET_IS_STATIC(0xFF),
					 "_CLLTK_FILE_OFFSET_IS_STATIC failed");
_CLLTK_STATIC_ASSERT(true == _CLLTK_FILE_OFFSET_IS_STATIC(0xFF + 1),
					 "_CLLTK_FILE_OFFSET_IS_STATIC failed");
_CLLTK_STATIC_ASSERT(true == _CLLTK_FILE_OFFSET_IS_STATIC(UINT64_MAX),
					 "_CLLTK_FILE_OFFSET_IS_STATIC failed");