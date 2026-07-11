// Copyright (c) 2026, International Business Machines
// SPDX-License-Identifier: BSD-2-Clause-Patent

// Span tracking with carryable ids in C: a span id is a plain value that can
// be passed as a function argument (or through any API), so sub-spans can be
// opened anywhere the id reaches - including other threads or processes.

#include "CommonLowLevelTracingKit/tracing/tracing.h"

#include <unistd.h>

CLLTK_TRACEBUFFER(SPANS_C, 4096)

// the parent span id arrives as an ordinary function argument
static void parse_input(clltk_span_id_t parent)
{
	clltk_span_id_t span = CLLTK_SPAN_BEGIN(SPANS_C, parent, "parse input");
	CLLTK_TRACEPOINT(SPANS_C, "parsing %d bytes", 512);
	usleep(1000);
	CLLTK_SPAN_END(SPANS_C, span);
}

static void write_output(clltk_span_id_t parent)
{
	clltk_span_id_t span = CLLTK_SPAN_BEGIN(SPANS_C, parent, "write output");
	usleep(500);
	CLLTK_SPAN_END(SPANS_C, span);
}

int main(void)
{
	clltk_span_id_t request = CLLTK_SPAN_BEGIN(SPANS_C, CLLTK_SPAN_NO_PARENT, "handle request");

	parse_input(request);
	write_output(request);

	CLLTK_SPAN_END(SPANS_C, request);

	// a span without an end: the decoder reports it as still open, which is
	// what a crashed process leaves behind
	(void)CLLTK_SPAN_BEGIN(SPANS_C, CLLTK_SPAN_NO_PARENT, "never finished");
	return 0;
}
