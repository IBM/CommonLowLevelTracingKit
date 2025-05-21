#!/usr/bin/python3
# Copyright (c) 2024, International Business Machines
# SPDX-License-Identifier: BSD-2-Clause-Patent

# %%
import os, sys
sys.path.append(os.path.dirname(os.path.realpath(__file__)))

import unittest
from helper.build_temp_target import process, Langauge

TRACEBUFFER_INFO_COUNT = 7

#%% test cases

class system_tests(unittest.TestCase):

    def test_random_constructor(self: unittest.TestCase):
        for language in [Langauge.C, Langauge.CPP]:
            with self.subTest(language=language):
                file_content = \
                    '''
                    #include "CommonLowLevelTracingKit/tracing.h"
                    CLLTK_TRACEBUFFER(BUFFER, 4096);
                    
                    __attribute__((constructor))
                    void constructor(void)
                    {
                        CLLTK_TRACEPOINT(BUFFER, "should be in tracebuffer");
                    }
                    
                    int main(void)
                    {
                        return 0;
                    }
                    '''
                tracepoints = process(file_content, language=language)
                self.assertEqual(1 + TRACEBUFFER_INFO_COUNT, len(tracepoints))
                self.assertEqual(tracepoints["formatted"][TRACEBUFFER_INFO_COUNT], "should be in tracebuffer")
                pass
            
    def test_misuse_of_tracebuffer(self: unittest.TestCase):
        for language in [Langauge.C, Langauge.CPP]:
            with self.subTest(language=language):
                file_content = \
                    '''
                    #include "CommonLowLevelTracingKit/tracing.h"
                    static _clltk_tracebuffer_handler_t _clltk_BUFFER = {};
                    
                    int main(void)
                    {
                        CLLTK_TRACEPOINT(BUFFER,"should be in tracebuffer");
                        return 0;
                    }
                    '''
                
                stderr :str = process(file_content, language=language, run_musst_fail=True)
                self.assertRegex(stderr, r'^\x1b\[1;31mclltk recoverable: could not open tracebuffer')
                pass
