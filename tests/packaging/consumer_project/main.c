/* Copyright (c) 2024, International Business Machines
 * SPDX-License-Identifier: BSD-2-Clause-Patent
 *
 * Minimal consumer program to verify CLLTK installs correctly
 * and can be used from a downstream CMake project.
 */

#include <CommonLowLevelTracingKit/tracing/tracing.h>

CLLTK_TRACEBUFFER(test_buf, 4096);

int main(void)
{
	CLLTK_TRACEPOINT(CLLTK_TRACEBUFFER_MACRO_VALUE(test_buf), "hello from consumer %d", 42);
	return 0;
}
