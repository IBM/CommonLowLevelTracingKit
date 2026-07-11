#!/usr/bin/python3
# Copyright (c) 2024, International Business Machines
# SPDX-License-Identifier: BSD-2-Clause-Patent

# %%
import os, sys

sys.path.insert(0, os.path.dirname(os.path.realpath(__file__)))

import unittest
from helpers.build_temp_target import process, Language

TRACEBUFFER_INFO_COUNT = 7

# %% test cases


class valid_build_tests(unittest.TestCase):
    def test_valid_file(self: unittest.TestCase):
        for language in [Language.C, Language.CPP]:
            with self.subTest(language=language):
                file_content = """
                    #include "CommonLowLevelTracingKit/tracing/tracing.h"
                    CLLTK_TRACEBUFFER(BUFFER, 4096);
                    int main(void)
                    {
                        CLLTK_TRACEPOINT(BUFFER, "%u", 42);
                        return 0;
                    }
                    """
                data = process(file_content, language=language)
                self.assertGreaterEqual(len(data["tracebuffer"].unique()), 1)
                self.assertEqual(len(data[data["formatted"] == "42"]), 1)
                pass

    def test_run_twice_same_language(self: unittest.TestCase):
        for language in [Language.C, Language.CPP]:
            with self.subTest(language=language):
                file_content = """
                    #include "CommonLowLevelTracingKit/tracing/tracing.h"
                    CLLTK_TRACEBUFFER(BUFFER, 4096);
                    int main(void)
                    {
                        CLLTK_TRACEPOINT(BUFFER, "%u", 42);
                        return 0;
                    }
                    """
                data = process(file_content, runs=2, language=language)
                self.assertGreaterEqual(len(data["tracebuffer"].unique()), 1)
                self.assertEqual(len(data[data["formatted"] == "42"]), 2)
                pass

    def test_wrapp(self: unittest.TestCase):
        for language in [Language.C, Language.CPP]:
            with self.subTest(language=language):
                file_content = """
                    #include "CommonLowLevelTracingKit/tracing/tracing.h"
                    CLLTK_TRACEBUFFER(BUFFER, 64);
                    int main(void)
                    {
                        for(int i = 0; i < 10; i++)
                            CLLTK_TRACEPOINT(BUFFER, "%u", 42);
                        return 0;
                    }
                    """
                data = process(file_content, language=language)
                self.assertLess(len(data[data["formatted"] == "42"]), 10)
                pass

    def test_tracepoint_in_comdat_and_plain_function_same_buffer(self: unittest.TestCase):
        """Regression test for the GCC >= 15.2 hard error:

            error: '_meta' causes a section type conflict with '_meta'
                   in section '_clltk_BUFFER_meta'

        A tracepoint inside a COMDAT context (inline function, function
        template, or class template member) places its meta object into a
        COMDAT-grouped ELF section, while a tracepoint in a plain function
        uses an ungrouped section of the same name. GCC 15.2+ refuses to mix
        the two in one translation unit. This only reproduces without LTO,
        which is how consumers compile against the installed headers.
        """
        common_part = """
            #include "CommonLowLevelTracingKit/tracing/tracing.h"
            CLLTK_TRACEBUFFER(BUFFER, 4096);
            void plain_function(void)
            {
                CLLTK_TRACEPOINT(BUFFER, "plain, longer format string %u", 42u);
            }
            """
        comdat_cases = {
            "inline function": """
                inline void comdat_function(void)
                {
                    CLLTK_TRACEPOINT(BUFFER, "comdat %d", 1);
                }
                int main(void)
                {
                    comdat_function();
                    plain_function();
                    return 0;
                }
                """,
            "function template": """
                template <typename T> void comdat_function(T value)
                {
                    CLLTK_TRACEPOINT(BUFFER, "comdat %d", (int)value);
                }
                int main(void)
                {
                    comdat_function(1);
                    plain_function();
                    return 0;
                }
                """,
            "class template member": """
                template <typename T> struct Wrapper
                {
                    static void trace(void)
                    {
                        CLLTK_TRACEPOINT(BUFFER, "comdat %d", 1);
                    }
                };
                int main(void)
                {
                    Wrapper<int>::trace();
                    plain_function();
                    return 0;
                }
                """,
            # A member function *defined inside* a class body is implicitly
            # inline, hence COMDAT. This used to fail to link (see the README
            # "Constrains" section); the COMDAT-safe discovery keeps it working.
            "in-class member function": """
                struct A
                {
                    void foo(void)
                    {
                        CLLTK_TRACEPOINT(BUFFER, "comdat %d", 1);
                    }
                };
                int main(void)
                {
                    A{}.foo();
                    plain_function();
                    return 0;
                }
                """,
        }
        for case_name, comdat_part in comdat_cases.items():
            with self.subTest(case=case_name):
                data = process(common_part + comdat_part, language=Language.CPP)
                self.assertEqual(len(data[data["formatted"] == "comdat 1"]), 1)
                self.assertEqual(
                    len(data[data["formatted"] == "plain, longer format string 42"]), 1
                )
                pass

    def test_spans(self: unittest.TestCase):
        """Spans write begin/end events with carryable ids: the decoder pairs
        them by id, resolves the parent relation, and reports spans without an
        end (e.g. after a crash) as still open."""
        for language in [Language.C, Language.CPP]:
            with self.subTest(language=language):
                file_content = """
                    #include "CommonLowLevelTracingKit/tracing/tracing.h"
                    CLLTK_TRACEBUFFER(BUFFER, 4096);
                    int main(void)
                    {
                        clltk_span_id_t outer =
                            CLLTK_SPAN_BEGIN(BUFFER, CLLTK_SPAN_NO_PARENT, "outer");
                        clltk_span_id_t inner = CLLTK_SPAN_BEGIN(BUFFER, outer, "inner");
                        CLLTK_TRACEPOINT(BUFFER, "inside %u", 42);
                        CLLTK_SPAN_END(BUFFER, inner);
                        CLLTK_SPAN_END(BUFFER, outer);
                        (void)CLLTK_SPAN_BEGIN(BUFFER, CLLTK_SPAN_NO_PARENT, "open");
                        return 0;
                    }
                    """
                data = process(file_content, language=language)
                formatted = data["formatted"].tolist()

                import re

                begins = {}
                ends = []
                for message in formatted:
                    m = re.match(r">>> (\S+) \[span (0x[0-9a-f]+)(?: parent (0x[0-9a-f]+))?\]", message)
                    if m:
                        begins[m.group(1)] = (m.group(2), m.group(3))
                    m = re.match(r"<<< \[span (0x[0-9a-f]+)\]", message)
                    if m:
                        ends.append(m.group(1))

                self.assertIn("inside 42", formatted)
                self.assertEqual({"outer", "inner", "open"}, set(begins))
                # inner's parent is outer; outer and open have no parent
                self.assertEqual(begins["inner"][1], begins["outer"][0])
                self.assertIsNone(begins["outer"][1])
                self.assertIsNone(begins["open"][1])
                # outer and inner ended, open did not
                self.assertEqual(sorted(ends),
                                 sorted([begins["outer"][0], begins["inner"][0]]))
                pass

    def test_fmt_tracepoints(self: unittest.TestCase):
        """CLLTK_TRACEPOINT_FMT uses {} placeholders (C++20 only), validated
        at compile time and rendered by the decoders."""
        file_content = """
            #include "CommonLowLevelTracingKit/tracing/tracing.h"
            CLLTK_TRACEBUFFER(BUFFER, 4096);
            int main()
            {
                CLLTK_TRACEPOINT_FMT(BUFFER, "loaded {} in {}ms", "module-a", 42);
                CLLTK_TRACEPOINT_FMT(BUFFER, "plain text no args");
                CLLTK_TRACEPOINT_FMT(BUFFER, "hex {:x} float {:.2f}", 255u, 3.5);
                return 0;
            }
            """
        data = process(file_content, language=Language.CPP)
        formatted = data["formatted"].tolist()
        self.assertIn("loaded module-a in 42ms", formatted)
        self.assertIn("plain text no args", formatted)
        self.assertIn("hex ff float 3.50", formatted)
        pass

    def test_empty(self: unittest.TestCase):
        for language in [Language.C, Language.CPP]:
            with self.subTest(language=language):
                file_content = """
                    #include "CommonLowLevelTracingKit/tracing/tracing.h"
                    CLLTK_TRACEBUFFER(BUFFER, 64);
                    int main(void)
                    {
                        volatile int i = 0;
                        if(i)
                            CLLTK_TRACEPOINT(BUFFER, "%u", 42);
                        return 0;
                    }
                    """
                data = process(file_content, language=language)
                self.assertEqual(len(data), TRACEBUFFER_INFO_COUNT)
                pass
