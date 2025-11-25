#!/usr/bin/python3
# Copyright (c) 2024, International Business Machines
# SPDX-License-Identifier: BSD-2-Clause-Patent

# %%
import os, sys
sys.path.append(os.path.dirname(os.path.realpath(__file__)))

import unittest
from helper.build_temp_target import process, Language

TRACEBUFFER_INFO_COUNT = 7

#%% test cases

class valid_build_tests(unittest.TestCase):

    def test_valid_file(self: unittest.TestCase):
        for language in [Language.C, Language.CPP]:
            with self.subTest(language=language):
                file_content = \
                    '''
                    #include "CommonLowLevelTracingKit/tracing/tracing.h"
                    CLLTK_TRACEBUFFER(BUFFER, 4096);
                    int main(void)
                    {
                        CLLTK_TRACEPOINT(BUFFER, "%u", 42);
                        return 0;
                    }
                    '''
                data = process(file_content, language=language)
                self.assertGreaterEqual(len(data["tracebuffer"].unique()),1)
                self.assertEqual(len(data[data["formatted"]=="42"]),1)
                pass



    def test_run_twice_same_language(self: unittest.TestCase):
        for language in [Language.C, Language.CPP]:
            with self.subTest(language=language):
                file_content = \
                    '''
                    #include "CommonLowLevelTracingKit/tracing/tracing.h"
                    CLLTK_TRACEBUFFER(BUFFER, 4096);
                    int main(void)
                    {
                        CLLTK_TRACEPOINT(BUFFER, "%u", 42);
                        return 0;
                    }
                    '''
                data = process(file_content, runs=2, language=language)
                self.assertGreaterEqual(len(data["tracebuffer"].unique()),1)
                self.assertEqual(len(data[data["formatted"]=="42"]),2)
                pass

    def test_wrapp(self: unittest.TestCase):
        for language in [Language.C, Language.CPP]:
            with self.subTest(language=language):
                file_content = \
                    '''
                    #include "CommonLowLevelTracingKit/tracing/tracing.h"
                    CLLTK_TRACEBUFFER(BUFFER, 64);
                    int main(void)
                    {
                        for(int i = 0; i < 10; i++)
                            CLLTK_TRACEPOINT(BUFFER, "%u", 42);
                        return 0;
                    }
                    '''
                data = process(file_content, language=language)
                self.assertLess(len(data[data["formatted"]=="42"]), 10)
                pass
    
    def test_empty(self: unittest.TestCase):
        for language in [Language.C, Language.CPP]:
            with self.subTest(language=language):
                file_content = \
                    '''
                    #include "CommonLowLevelTracingKit/tracing/tracing.h"
                    CLLTK_TRACEBUFFER(BUFFER, 64);
                    int main(void)
                    {
                        volatile int i = 0;
                        if(i)
                            CLLTK_TRACEPOINT(BUFFER, "%u", 42);
                        return 0;
                    }
                    '''
                data = process(file_content, language=language)
                self.assertEqual(len(data), TRACEBUFFER_INFO_COUNT)
                pass
