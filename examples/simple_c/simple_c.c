// Copyright (c) 2024, International Business Machines
// SPDX-License-Identifier: BSD-2-Clause-Patent

#include "CommonLowLevelTracingKit/tracing.h"

CLLTK_TRACEBUFFER(SIMPLE_C, 1024)

int main(void)
{
	CLLTK_TRACEPOINT(SIMPLE_C, "a simple example with some args %%u=32=%u %%f=1.33e-3=%f %%s=%s",
					 32u, 1.33e-3, "string arg");
	return 0;
}
