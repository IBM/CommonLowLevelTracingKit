// Copyright (c) 2026, International Business Machines
// SPDX-License-Identifier: BSD-2-Clause-Patent

// Deterministic identity shim for golden fixture generation.
//
// The tracing library stamps every entry with timestamp/pid/tid via the three
// abstraction symbols below, and clltk_next_span_id() derives its per-process
// salt from timestamp ^ pid. Linked ahead of the tracing static archive, these
// strong definitions satisfy the references so the archive's info.o is never
// pulled, freezing every timestamp, pid, tid, and span id. The generated trace
// is then byte-identical run to run, which is what lets the golden gate decide
// "did the format change?" with a plain byte compare.
//
// Values are arbitrary fixed constants; only their stability matters.

#include <stdint.h>

uint64_t info_get_timestamp_ns(void)
{
	return 1700000000000000000ULL;
}

uint32_t info_get_process_id(void)
{
	return 4242;
}

uint32_t info_get_thread_id(void)
{
	return 4343;
}
