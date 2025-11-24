// Copyright (c) 2024, International Business Machines
// SPDX-License-Identifier: BSD-2-Clause-Patent

#include <linux/elf.h>
#include <linux/module.h>
#include <linux/moduleloader.h>
#include <linux/string.h>
#include <linux/types.h>

#include "CommonLowLevelTracingKit/tracing/tracing.h"
#include "kthreadtest.h"
#include "more.h"

CLLTK_TRACEBUFFER(BufferSimpleC, 1024 * 1024);

static inline void foo(void)
{
	CLLTK_TRACEPOINT(BufferMoreH, "%s", __PRETTY_FUNCTION__);
	CLLTK_TRACEPOINT(BufferSimpleC, "%s", __PRETTY_FUNCTION__);
}

static void bar(void)
{
	CLLTK_TRACEPOINT(BufferMoreH, "%s", __PRETTY_FUNCTION__);
	CLLTK_TRACEPOINT(BufferSimpleC, "%s", __PRETTY_FUNCTION__);
	for (uint64_t index = 0; index < 1024; index++)
		CLLTK_TRACEPOINT(BufferSimpleC, "func bar %llu", index);
}

static int __init init_clltk_simple_trace(void)
{
	CLLTK_TRACEPOINT(BufferMoreH, "%s", __PRETTY_FUNCTION__);
	CLLTK_TRACEPOINT(BufferSimpleC, "%s", __PRETTY_FUNCTION__);
	const char str[] = "__PRETTY_FUNCTION__";
	const size_t str_size = strlen(str);
	CLLTK_TRACEPOINT_DUMP(BufferSimpleC, "dump ", str, str_size);
	foo();
	bar();
	more();
	more_inline();
	kthreadtest();
	return 0;
}

static void __exit exit_clltk_simple_trace(void) {}

module_init(init_clltk_simple_trace);
module_exit(exit_clltk_simple_trace);
MODULE_LICENSE("GPL");
