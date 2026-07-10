// Copyright (c) 2026, International Business Machines
// SPDX-License-Identifier: BSD-2-Clause-Patent

// fmt-style tracepoints (C++20): format strings use {} placeholders and are
// validated against the argument types at compile time via std::format_string.
// Same speed and mechanics as CLLTK_TRACEPOINT - only the syntax differs.

#include "CommonLowLevelTracingKit/tracing/tracing.h"

#include <string>

CLLTK_TRACEBUFFER(FMT_CPP, 4096)

int main()
{
	CLLTK_TRACEPOINT_FMT(FMT_CPP, "loaded {} in {}ms", "module-a", 42);
	CLLTK_TRACEPOINT_FMT(FMT_CPP, "hex {:x} padded {:>8} float {:.2f}", 255u, 7, 3.5);

	// a char* argument is always recorded as a string; record the pointer
	// value by casting to void*
	const char *name = "pointer-vs-string";
	CLLTK_TRACEPOINT_FMT(FMT_CPP, "string {} pointer {}", name, (const void *)name);

	// mixing with printf-style tracepoints in the same buffer is fine
	CLLTK_TRACEPOINT(FMT_CPP, "printf style %s", "still works");
	return 0;
}
