// Copyright (c) 2024, International Business Machines
// SPDX-License-Identifier: BSD-2-Clause-Patent

#ifndef __HEADER__MORE__
#define __HEADER__MORE__

#include "CommonLowLevelTracingKit/tracing.h"

CLLTK_TRACEBUFFER(BufferMoreH, 1024);

void more(void);

static inline void more_inline(void)
{
	CLLTK_TRACEBUFFER(BufferMoreInline, 1024);
	CLLTK_TRACEPOINT(BufferMoreH, "%s", __PRETTY_FUNCTION__);
	CLLTK_TRACEPOINT(BufferMoreInline, "%s", __PRETTY_FUNCTION__);
}

#endif // !__HEADER__MORE__