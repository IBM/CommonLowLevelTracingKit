// Copyright (c) 2024, International Business Machines
// SPDX-License-Identifier: BSD-2-Clause-Patent

#include "CommonLowLevelTracingKit/tracing.h"
#include <stdio.h>
#include <stdlib.h>

CLLTK_TRACEBUFFER(COMPLEX_CPP, 100 * 1024);

void macro_as_tracebuffer_name(void);
void different_formats(void);
void dynamic_tracing(void);
void typedef_example(void);
void inline_functions(void);
void namespace_functions(void);
void tracepoint_dump(void);
void template_function(void);
void corner_cases(void);

int main(int argc, char *argv[])
{
	int LOOPS = 1;
	if (argc == 2) {
		char *a = argv[1];
		LOOPS = atoi(a);
	}

	printf("LOOPS %lu\n", (size_t)LOOPS);

	CLLTK_TRACEPOINT(COMPLEX_CPP, "LOOPS %lu", (size_t)LOOPS);
	for (size_t i = 0; i < (size_t)LOOPS; i++) {
		CLLTK_TRACEPOINT(COMPLEX_CPP, "loop counter %lu", i);
		corner_cases();
		different_formats();
		macro_as_tracebuffer_name();
		dynamic_tracing();
		typedef_example();
		inline_functions();
		namespace_functions();
		tracepoint_dump();
		template_function();
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

CLLTK_TRACEBUFFER(FORMAT_TEST, 4096);
void different_formats(void) // for python test
{

#define FORMAT_TEST(NAME, EXPECT, FORMAT, ...)                                        \
	CLLTK_TRACEPOINT(FORMAT_TEST, "{\"tracebuffer\":\"FORMAT_TEST\",\"name\":\"" NAME \
								  "\",\"expected\":\"" EXPECT "\",\"got\":\"" FORMAT  \
								  "\"}" __VA_OPT__(, __VA_ARGS__))

	FORMAT_TEST("", "string argument", "%s", "string argument");
	FORMAT_TEST("", "01", "%02lu", 1lu);
	FORMAT_TEST("", "01", "%02u", (bool)1);
	FORMAT_TEST("", "A", "%X", 0xA);
	FORMAT_TEST("", "a", "%x", 0xA);
	FORMAT_TEST("", "0a", "%02x", 0xA);
	FORMAT_TEST("", "001", "%03lu", 1lu);
	FORMAT_TEST("", "1.0e+01", "%.1e", 1e1);
	FORMAT_TEST("", "1.0E+01", "%.1E", 1e1);
	FORMAT_TEST("", "s0 s1", "%s %s", "s0", "s1");
	FORMAT_TEST("pointer test", "0x0", "%p", (void *)NULL);
	FORMAT_TEST("pointer test", "0x0", "%p", (int *)NULL);
	{
		FILE *fh = (FILE *)0x42;
		FORMAT_TEST("pointer test", "0x42", "%p", fh);
	}
	{
		int *i = (int *)0x42;
		FORMAT_TEST("pointer test", "0x42", "%p", i);
	}
	{
		enum { test_enum_value = 1 };
		FORMAT_TEST("enum", "1", "%u", test_enum_value);
		FORMAT_TEST("enum", "1", "%x", test_enum_value);
		FORMAT_TEST("enum", "1", "%d", test_enum_value);
	}

	{
		enum { test_enum_value = 1 };
		FORMAT_TEST("enum with underlying type", "1", "%u", test_enum_value);
		FORMAT_TEST("enum with underlying type", "1", "%x", test_enum_value);
		FORMAT_TEST("enum with underlying type", "1", "%d", test_enum_value);
	}

	{
		enum class test_enum { value = 1 };
		FORMAT_TEST("scoped enum", "1", "%u", test_enum::value);
		FORMAT_TEST("scoped enum", "1", "%x", test_enum::value);
		FORMAT_TEST("scoped enum", "1", "%d", test_enum::value);
	}

	{
		enum class test_enum : int { value = 1 };
		FORMAT_TEST("scoped enum with underlying type", "1", "%u", test_enum::value);
		FORMAT_TEST("scoped enum with underlying type", "1", "%x", test_enum::value);
		FORMAT_TEST("scoped enum with underlying type", "1", "%d", test_enum::value);
	}

	FORMAT_TEST("empty string", "", "");

#undef TEST
}

CLLTK_TRACEBUFFER(COMPLEX_CPP_A, 4096);
CLLTK_TRACEBUFFER(COMPLEX_CPP_B, 4096);
#define COMPLEX_CPP_C COMPLEX_CPP_MACRO
CLLTK_TRACEBUFFER(COMPLEX_CPP_C, 4096);
void macro_as_tracebuffer_name(void)
{

#define _TB_ COMPLEX_CPP_A
	CLLTK_TRACEPOINT(_TB_, "{\"tracebuffer\":\"%s\"}", "COMPLEX_CPP_A");
#undef _TB_

#define _TB0_ COMPLEX_CPP_A
#define _TB_ _TB0_
	CLLTK_TRACEPOINT(_TB_, "{\"tracebuffer\":\"%s\"}", "COMPLEX_CPP_A");
#undef _TB0_
#undef _TB_

#define _TB_ _TB1_
#define _TB1_ _TB0_
#define _TB0_ COMPLEX_CPP_A
	CLLTK_TRACEPOINT(_TB_, "{\"tracebuffer\":\"%s\"}", "COMPLEX_CPP_A");
#undef _TB1_
#undef _TB0_
#undef _TB_

#define _TB_ COMPLEX_CPP_B
	CLLTK_TRACEPOINT(_TB_, "{\"tracebuffer\":\"%s\"}", "COMPLEX_CPP_B");
#undef _TB_

#define _TB_ COMPLEX_CPP_C
	CLLTK_TRACEPOINT(_TB_, "{\"tracebuffer\":\"%s\"}", "COMPLEX_CPP_A");
#undef _TB_
}

void dynamic_tracing(void)
{
	clltk_dynamic_tracebuffer_creation("SIMPLE_CPP_dyn", 4096);
	clltk_dynamic_tracepoint_execution("SIMPLE_CPP_dyn", __FILE__, __LINE__, 0, 0,
									   "dynamic tracepoint with two args %s %lu", "arg0", 10lu);
	CLLTK_DYN_TRACEPOINT("SIMPLE_CPP_dyn", "{\"tracebuffer\":\"%s\"}", "SIMPLE_CPP_dyn");
}

void typedef_example(void)
{
	typedef int raw;
	raw value = 1;
	CLLTK_TRACEPOINT(COMPLEX_CPP, "%u", value);
}

CLLTK_TRACEBUFFER(INLINE_FUNCTIONS_CPP, 4096);
inline void inline_func(void)
{
	CLLTK_DYN_TRACEPOINT("INLINE_FUNCTIONS_CPP", "CLLTK_DYN_TRACEPOINT in %s", __PRETTY_FUNCTION__);
	clltk_dynamic_tracepoint_execution(
		"INLINE_FUNCTIONS_CPP", __FILE__, __LINE__, 0, 0,
		"CommonLowLevelTracingKit_dynamic_tracepoint_execution in %s", __PRETTY_FUNCTION__);
}
static inline void static_inline_func(void)
{
	CLLTK_TRACEPOINT(INLINE_FUNCTIONS_CPP, "CLLTK_DYN_TRACEPOINT in %s", __PRETTY_FUNCTION__);
	clltk_dynamic_tracepoint_execution(
		"INLINE_FUNCTIONS_CPP", __FILE__, __LINE__, 0, 0,
		"CommonLowLevelTracingKit_dynamic_tracepoint_execution in %s", __PRETTY_FUNCTION__);
}
void inline_functions(void)
{
	inline_func();
	static_inline_func();
}

CLLTK_TRACEBUFFER(NAMESPACE_FUNCTIONS, 4096);
namespace test
{
void _namespace_function(void)
{
	CLLTK_TRACEPOINT(NAMESPACE_FUNCTIONS, "arg = %s", "arg");
}
} // namespace test
void namespace_functions(void)
{
	test::_namespace_function();
}

CLLTK_TRACEBUFFER(DUMP_CPP, 4096);
void tracepoint_dump(void)
{
	char i[] = "{\"tracebuffer\":\"DUMP\"}";
	CLLTK_TRACEPOINT_DUMP(DUMP_CPP, "dump message", i, sizeof(i) - 1);
}

CLLTK_TRACEBUFFER(DESTRUCTOR_CPP, 4096);
__attribute__((destructor(103))) void destructor103(void)
{
	CLLTK_TRACEPOINT(DESTRUCTOR_CPP, "%s", __PRETTY_FUNCTION__);
}
__attribute__((destructor(102))) void destructor102(void)
{
	CLLTK_TRACEPOINT(DESTRUCTOR_CPP, "%s", __PRETTY_FUNCTION__);
}
__attribute__((destructor(101))) void destructor101(void)
{
	CLLTK_TRACEPOINT(DESTRUCTOR_CPP, "%s", __PRETTY_FUNCTION__);
}

struct TestClass {
	TestClass(void);
	~TestClass();
};

TestClass::TestClass(void)
{
	CLLTK_TRACEPOINT(DESTRUCTOR_CPP, "%s", __PRETTY_FUNCTION__);
}
TestClass::~TestClass(void)
{
	CLLTK_TRACEPOINT(DESTRUCTOR_CPP, "%s", __PRETTY_FUNCTION__);
}
static TestClass object{};

CLLTK_TRACEBUFFER(TEMPLATE, 4096);
template <typename Type> struct Template {
	using type = Type;
	Template(void) { CLLTK_TRACEPOINT(TEMPLATE, "%s", __PRETTY_FUNCTION__); }
	void normal_func(void) { CLLTK_TRACEPOINT(TEMPLATE, "%s", __PRETTY_FUNCTION__); }
	static void static_func(void) { CLLTK_TRACEPOINT(TEMPLATE, "%s", __PRETTY_FUNCTION__); }
};
static Template<int> t;
void template_function(void)
{
	t.normal_func();
	t.static_func();
	Template<double>::static_func();
	Template<double>{}.normal_func();
}
