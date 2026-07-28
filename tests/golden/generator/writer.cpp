// Copyright (c) 2026, International Business Machines
// SPDX-License-Identifier: BSD-2-Clause-Patent

// Golden fixture writer: exercises every tracepoint kind the library supports
// -- printf, dump, dynamic (runtime-resolved), spans, and fmt (C++20) -- and
// every printf argument type, once, with deterministic values into a single
// GOLDEN buffer. Compiled as C++ because fmt tracepoints are C++20-only; the
// other kinds compile identically from C. The produced GOLDEN.clltk_trace is
// committed as a golden fixture; pid/tid/timestamps are frozen inside the file
// by frozen_info.c so it is byte-identical run to run.
#include "CommonLowLevelTracingKit/tracing/tracing.h"
#include <cstdint>

CLLTK_TRACEBUFFER(GOLDEN, 8192)

volatile int never = 0;

int main()
{
	// printf tracepoints: one per argument type
	CLLTK_TRACEPOINT(GOLDEN, "plain int %d", 42);
	CLLTK_TRACEPOINT(GOLDEN, "u8 %u u16 %u u32 %u", (uint8_t)8, (uint16_t)1616, (uint32_t)323232);
	CLLTK_TRACEPOINT(GOLDEN, "i64 %ld u64 %lu", (int64_t)-64646464,
					 (uint64_t)18446744073709551615ull);
	CLLTK_TRACEPOINT(GOLDEN, "float %f double %f", 3.5f, -2.25);
	CLLTK_TRACEPOINT(GOLDEN, "string %s", "golden string");
	CLLTK_TRACEPOINT(GOLDEN, "pointer %p", (void *)0x123456789abcull);

	// dump tracepoint
	const uint8_t dump_data[8] = {0xde, 0xad, 0xbe, 0xef, 0x01, 0x02, 0x03, 0x04};
	CLLTK_TRACEPOINT_DUMP(GOLDEN, "golden dump", dump_data, sizeof(dump_data));

	// dynamic (runtime-resolved) tracepoint into the same GOLDEN buffer
	CLLTK_DYN_TRACEPOINT("GOLDEN", "golden dyn %d", 7);

	// fmt tracepoints (C++20)
	CLLTK_TRACEPOINT_FMT(GOLDEN, "loaded {} in {}ms", "module-a", 42);
	CLLTK_TRACEPOINT_FMT(GOLDEN, "plain text no args");
	CLLTK_TRACEPOINT_FMT(GOLDEN, "hex {:x} float {:.2f}", 255u, 3.5);

	// spans: outer with an inner child (both ended) plus one span left open
	{
		clltk_span_id_t outer = CLLTK_SPAN_BEGIN(GOLDEN, CLLTK_SPAN_NO_PARENT, "golden outer span");
		clltk_span_id_t inner = CLLTK_SPAN_BEGIN(GOLDEN, outer, "golden inner span");
		CLLTK_SPAN_END(GOLDEN, inner);
		CLLTK_SPAN_END(GOLDEN, outer);
		(void)CLLTK_SPAN_BEGIN(GOLDEN, CLLTK_SPAN_NO_PARENT, "golden open span");
	}

	// a never-fired tracepoint: registered in the definition section but no entry
	if (never) {
		CLLTK_TRACEPOINT(GOLDEN, "never fired %d", 1);
	}
	return 0;
}
