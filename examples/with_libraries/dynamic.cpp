// Copyright (c) 2024, International Business Machines
// SPDX-License-Identifier: BSD-2-Clause-Patent

#include "CommonLowLevelTracingKit/tracing/tracing.h"
#include "template.hpp"
#include "with_libraries.hpp"

CLLTK_TRACEBUFFER(with_libraries_dynamic, 1024);

__attribute__((constructor(102), visibility("hidden"), used)) void constructor(void)
{
	CLLTK_TRACEPOINT(with_libraries, "dynamic library constructor");
	CLLTK_TRACEPOINT(with_libraries_dynamic, "dynamic library constructor");
}

__attribute__((destructor(104), visibility("hidden"), used)) void destructor(void)
{
	CLLTK_TRACEPOINT(with_libraries, "dynamic library destructor");
	CLLTK_TRACEPOINT(with_libraries_dynamic, "dynamic library destructor");
}

void run(void)
{
	auto a = template_class::template_class<int>();
	a.run();
}

extern "C" {
void dynamic_lib(void)
{
	CLLTK_TRACEPOINT(with_libraries, "dynamic library function");
	CLLTK_TRACEPOINT(with_libraries_dynamic, "dynamic library function");
	run();
}
}
