// Copyright (c) 2024, International Business Machines
// SPDX-License-Identifier: BSD-2-Clause-Patent

#pragma once

#include "CommonLowLevelTracingKit/tracing/tracing.h"

CLLTK_TRACEBUFFER(with_libraries, 1024);

void static_lib(void);
void shared_lib(void);
