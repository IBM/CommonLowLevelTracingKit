// Copyright (c) 2024, International Business Machines
// SPDX-License-Identifier: BSD-2-Clause-Patent

#include "with_libraries.hpp"
#include "template.hpp"
#include <dlfcn.h>

CLLTK_TRACEBUFFER(with_libraries_main, 1024);

__attribute__((constructor(102), used)) static void constructor(void)
{
	CLLTK_TRACEPOINT(with_libraries, "main constructor");
	CLLTK_TRACEPOINT(with_libraries_main, "main constructor");
}

__attribute__((destructor(104), used)) static void destructor(void)
{
	CLLTK_TRACEPOINT(with_libraries, "main destructor");
	CLLTK_TRACEPOINT(with_libraries_main, "main destructor");
}

int main(void)
{

	CLLTK_TRACEPOINT(with_libraries, "from main");
	CLLTK_TRACEPOINT(with_libraries_main, "from main");
	static_lib();
	shared_lib();

	void *handle = dlopen("libexample-with_libraries-dynamic.so", RTLD_LAZY);
	void (*dynamic_lib)(void) = (void (*)(void))dlsym(handle, "dynamic_lib");
	dynamic_lib();
	dlclose(handle);

	auto a = template_class::template_class<int>();
	a.run();

	return 0;
}
