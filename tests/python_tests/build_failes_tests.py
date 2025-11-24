#!/usr/bin/python3
# Copyright (c) 2024, International Business Machines
# SPDX-License-Identifier: BSD-2-Clause-Patent

# %%
import os, sys
sys.path.append(os.path.dirname(os.path.realpath(__file__)))
import unittest
from helper.build_temp_target import process


#%% test cases

class failing_build_tests(unittest.TestCase):

    def test_build_faild(self:unittest.TestCase):
        file_content = "#error \"error\"\nint main(void){return 0;}"
        self.assertEqual(None, process(file_content, build_musst_fail=True))
        pass

    def test_empty_file(self:unittest.TestCase):
        file_content = ""
        self.assertEqual(None, process(file_content, build_musst_fail=True))
        pass

    def test_missing_buffer_size(self: unittest.TestCase):
        file_content = \
            '''
            #include "CommonLowLevelTracingKit/tracing/tracing.h"
            CLLTK_TRACEBUFFER(BUFFER);
            int main(void){return 0;}
            '''
        self.assertEqual(None, process(file_content, build_musst_fail=True))
        pass

    def test_tracebuffer_as_string(self: unittest.TestCase):
        file_content = \
            '''
            #include "CommonLowLevelTracingKit/tracing/tracing.h"
            CLLTK_TRACEBUFFER("BUFFER", 4096);
            int main(void){return 0;}
            '''
        self.assertEqual(None, process(file_content, build_musst_fail=True))
        pass

    def test_11args(self: unittest.TestCase):
        file_content = \
            '''
            #include "CommonLowLevelTracingKit/tracing/tracing.h"
            CLLTK_TRACEBUFFER(BUFFER, 4096);
            int main(void)
            {
                CLLTK_TRACEPOINT(BUFFER,"0%u 1%u 2%u 3%u 4%u 5%u 6%u 7%u 8%u 9%u 10%u",
                0,1,2,3,4,5,6,7,8,9,10);
                return 0;
            }
            '''
        self.assertEqual(None, process(file_content, build_musst_fail=True))
        pass

    def test_args_count_mismatch(self: unittest.TestCase):
        file_content = \
            '''
            #include "CommonLowLevelTracingKit/tracing/tracing.h"
            CLLTK_TRACEBUFFER(BUFFER, 4096);
            int main(void)
            {
                CLLTK_TRACEPOINT(BUFFER,"0%u 1%u 2%u 3%u 4%u 5%u"
                0,1,2,3,4,5,6);
                return 0;
            }
            '''
        self.assertEqual(None, process(file_content, build_musst_fail=True))
        pass

    def test_buffer_name_mismatch(self: unittest.TestCase):
        file_content = \
            '''
            #include "CommonLowLevelTracingKit/tracing/tracing.h"
            CLLTK_TRACEBUFFER(BUFFER_, 4096);
            int main(void)
            {
                CLLTK_TRACEPOINT(_BUFFER, " ");
                return 0;
            }
            '''
        self.assertEqual(None, process(file_content, build_musst_fail=True))
        pass


