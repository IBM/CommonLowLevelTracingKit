// Copyright (c) 2024, International Business Machines
// SPDX-License-Identifier: BSD-2-Clause-Patent

#include "CommonLowLevelTracingKit/tracing.h"

CLLTK_TRACEBUFFER(SIMPLE_CPP, 1024)

int main(void)
{
	CLLTK_TRACEPOINT(SIMPLE_CPP, "a simple example with some args %%u=32=%d %%f=1.33e-3=%f %%s=%s",
					 32, 1.33e-3, "string arg");
	return 0;
}
