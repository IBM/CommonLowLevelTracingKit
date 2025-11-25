// Copyright (c) 2024, International Business Machines
// SPDX-License-Identifier: BSD-2-Clause-Patent

#include "more.h"

#include "CommonLowLevelTracingKit/tracing/tracing.h"

CLLTK_TRACEBUFFER(BufferMoreC, 1024);

void more(void)
{
	CLLTK_TRACEPOINT(BufferMoreH, "%s", __PRETTY_FUNCTION__);
	CLLTK_TRACEPOINT(BufferMoreC, "%s", __PRETTY_FUNCTION__);
	more_inline();
}