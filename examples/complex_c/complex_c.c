// Copyright (c) 2024, International Business Machines
// SPDX-License-Identifier: BSD-2-Clause-Patent

#include "CommonLowLevelTracingKit/tracing.h"
#include <stdio.h>
#include <stdlib.h>

CLLTK_TRACEBUFFER(COMPLEX_C, 100 * 1024)

void macro_as_tracebuffer_name(void);
void different_formats(void);
void dynamic_tracing(void);
void corner_cases(void);

int main(int argc, char *argv[])
{
	int LOOPS = 1;
	if (argc == 2) {
		_CLLTK_PRAGMA_DIAG(push)
		_CLLTK_PRAGMA_DIAG(ignored "-Wunsafe-buffer-usage")
		char *a = argv[1];
		_CLLTK_PRAGMA_DIAG(pop)
		LOOPS = atoi(a);
	}

	printf("LOOPS %lu\n", (size_t)LOOPS);

	CLLTK_TRACEPOINT(COMPLEX_C, "LOOPS %lu", (size_t)LOOPS);
	for (size_t i = 0; i < (size_t)LOOPS; i++) {
		CLLTK_TRACEPOINT(COMPLEX_C, "loop counter %lu", i);
		corner_cases();
		different_formats();
		macro_as_tracebuffer_name();
		dynamic_tracing();
	}

	return 0;
}

CLLTK_TRACEBUFFER(CORNER_CASES, 4096);
void corner_cases(void)
{
	volatile char str[] = "some string";
	CLLTK_TRACEPOINT(CORNER_CASES, "should be a pointer, not a string = %p", (volatile char *)str);
#pragma GCC diagnostic ignored "-Wformat"
	CLLTK_TRACEPOINT(CORNER_CASES, "should be a string, not a pointer = %s", (volatile void *)str);
#pragma GCC diagnostic warning "-Wformat"
}

CLLTK_TRACEBUFFER(FORMAT_TEST, 4096)
void different_formats(void) // for python test
{

#define TEST(NAME, EXPECT, FORMAT, ...)                                           \
	CLLTK_TRACEPOINT(FORMAT_TEST, "{\"name\":\"" NAME "\",\"expected\":\"" EXPECT \
								  "\",\"got\":\"" FORMAT "\"}" __VA_OPT__(, __VA_ARGS__))

	TEST("", "string argument", "%s", "string argument");
	TEST("", "01", "%02lu", 1lu);
	TEST("", "A", "%X", 0xAu);
	TEST("", "a", "%x", 0xAu);
	TEST("", "0a", "%02x", 0xAu);
	TEST("", "001", "%03lu", 1lu);
	TEST("", "1.0e+01", "%.1e", 1e1);
	TEST("", "1.0E+01", "%.1E", 1e1);
	TEST("", "s0 s1", "%s %s", "s0", "s1");
	enum : unsigned { test_enum_uint = 1 };
	enum : signed { test_enum_sint = -1 };
	TEST("", "1", "%u", test_enum_uint);
	TEST("", "1", "%x", test_enum_uint);
	TEST("", "-1", "%d", test_enum_sint);

#undef TEST
}

CLLTK_TRACEBUFFER(COMPLEX_C_A, 4096)
CLLTK_TRACEBUFFER(COMPLEX_C_B, 4096)
void macro_as_tracebuffer_name(void)
{
#define _TB_ COMPLEX_C_A
#define _TP_ CLLTK_TRACEPOINT(_TB_, "tracepoint in COMPLEX_C_A")
	_TP_;
#undef _TB_
#define _TB_ COMPLEX_C_B
#undef _TP_
#define _TP_ CLLTK_TRACEPOINT(_TB_, "tracepoint in COMPLEX_C_B")
	_TP_;
#undef _TB_
#undef _TP_
}

void dynamic_tracing(void)
{
	clltk_dynamic_tracebuffer_creation("SIMPLE_C_dyn", 1024);
	clltk_dynamic_tracepoint_execution("SIMPLE_C_dyn", __FILE__, __LINE__, 0, 0,
									   "dynamic tracepoint with two args %s %lu", "arg0", 10lu);
}

CLLTK_TRACEBUFFER(DESTRUCTOR, 4096)
__attribute__((destructor(103))) void destructor103(void)
{
	CLLTK_TRACEPOINT(DESTRUCTOR, "%s", __PRETTY_FUNCTION__);
}
__attribute__((destructor(102))) void destructor102(void)
{
	CLLTK_TRACEPOINT(DESTRUCTOR, "%s", __PRETTY_FUNCTION__);
}
__attribute__((destructor(101))) void destructor101(void)
{
	CLLTK_TRACEPOINT(DESTRUCTOR, "%s", __PRETTY_FUNCTION__);
}
