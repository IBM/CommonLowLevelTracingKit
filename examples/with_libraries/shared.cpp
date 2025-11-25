// Copyright (c) 2024, International Business Machines
// SPDX-License-Identifier: BSD-2-Clause-Patent

#include "CommonLowLevelTracingKit/tracing/tracing.h"
#include "template.hpp"
#include "with_libraries.hpp"

CLLTK_TRACEBUFFER(with_libraries_shared, 1024);

__attribute__((constructor(102), visibility("hidden"), used)) void constructor(void)
{
	CLLTK_TRACEPOINT(with_libraries, "shared library constructor");
	CLLTK_TRACEPOINT(with_libraries_shared, "shared library constructor");
}

__attribute__((destructor(104), visibility("hidden"), used)) void destructor(void)
{
	CLLTK_TRACEPOINT(with_libraries, "shared library destructor");
	CLLTK_TRACEPOINT(with_libraries_shared, "shared library destructor");
}

void shared_lib(void)
{
	CLLTK_TRACEPOINT(with_libraries, "shared library function");
	CLLTK_TRACEPOINT(with_libraries_shared, "shared library function");
	auto a = template_class::template_class<int>();
	a.run();
}
