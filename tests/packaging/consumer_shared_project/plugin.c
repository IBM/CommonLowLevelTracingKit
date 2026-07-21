/* Copyright (c) 2024, International Business Machines
 * SPDX-License-Identifier: BSD-2-Clause-Patent
 *
 * A consumer shared library that traces. Expanding the tracepoint macros here
 * emits the _clltk_<buffer>_metaptr section into this .so; that section must be
 * writable so the dynamic linker relocates its absolute pointers without a text
 * relocation (DT_TEXTREL). The packaging test loads this .so and asserts it is
 * text-relocation free.
 */

#include <CommonLowLevelTracingKit/tracing/tracing.h>

CLLTK_TRACEBUFFER(consumer_plugin_buf, 4096);

void plugin_emit(int value)
{
	CLLTK_TRACEPOINT(CLLTK_TRACEBUFFER_MACRO_VALUE(consumer_plugin_buf), "plugin traced value %d",
					 value);
}
