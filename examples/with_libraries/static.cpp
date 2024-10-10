// Copyright (c) 2024, International Business Machines
// SPDX-License-Identifier: BSD-2-Clause-Patent

#include "CommonLowLevelTracingKit/tracing.h"
#include "template.hpp"
#include "with_libraries.hpp"

CLLTK_TRACEBUFFER(with_libraries_static, 1024);

__attribute__((constructor(102), used)) static void constructor(void)
{
	CLLTK_TRACEPOINT(with_libraries, "static library constructor");
	CLLTK_TRACEPOINT(with_libraries_static, "static library constructor");
}

__attribute__((destructor(104), used)) static void destructor(void)
{
	CLLTK_TRACEPOINT(with_libraries, "static library destructor");
	CLLTK_TRACEPOINT(with_libraries_static, "static library destructor");
}

void static_lib(void)
{
	CLLTK_TRACEPOINT(with_libraries, "static library function");
	CLLTK_TRACEPOINT(with_libraries_static, "static library function");
	auto a = template_class::template_class<int>();
	a.run();
}
