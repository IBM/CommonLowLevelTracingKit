// Copyright (c) 2026, International Business Machines
// SPDX-License-Identifier: BSD-2-Clause-Patent

// Golden fixture writer for fmt-style tracepoints (C++20 only, meta type 5).
// Deterministic values; the produced GOLDEN_FMT.clltk_trace is committed.
#include "CommonLowLevelTracingKit/tracing/tracing.h"

CLLTK_TRACEBUFFER(GOLDEN_FMT, 4096)

int main()
{
	CLLTK_TRACEPOINT_FMT(GOLDEN_FMT, "loaded {} in {}ms", "module-a", 42);
	CLLTK_TRACEPOINT_FMT(GOLDEN_FMT, "plain text no args");
	CLLTK_TRACEPOINT_FMT(GOLDEN_FMT, "hex {:x} float {:.2f}", 255u, 3.5);
	return 0;
}
