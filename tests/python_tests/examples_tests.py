#!/usr/bin/python3
# Copyright (c) 2024, International Business Machines
# SPDX-License-Identifier: BSD-2-Clause-Patent

# %%

import os, sys
sys.path.append(os.path.dirname(os.path.realpath(__file__)))

from unittest import TestCase
from helper.build_examples_helper import unittest
import pandas as pd
import pathlib
import re
import json

TRACEBUFFER_INFO_COUNT = 7

class simple_c(unittest):
    target = "example-simple_c"
    root = pathlib.Path().resolve()

    def test(self: unittest):
        data: pd.DataFrame = self.decoded
        self.assertEqual(
            1 + TRACEBUFFER_INFO_COUNT, len(data), "not just one traceentry in tracebuffer")
        self.assertIn("SIMPLE_C", data.tracebuffer.unique(),
                      "wrong tracebuffer name")
        msg: str = data.formatted[TRACEBUFFER_INFO_COUNT]
        self.assertTrue(msg.startswith("a simple example with some args "))
        file: str = data.file[TRACEBUFFER_INFO_COUNT]
        self.assertTrue(file.endswith("simple_c.c"))

    pass


class simple_cpp(unittest):
    target = "example-simple_cpp"
    root = pathlib.Path().resolve()

    def test(self: unittest):
        data: pd.DataFrame = self.decoded
        self.assertEqual(
            1 + TRACEBUFFER_INFO_COUNT, len(data), "not just one traceentry in tracebuffer")
        self.assertIn("SIMPLE_CPP", data.tracebuffer.unique(),
                      "wrong tracebuffer name")
        msg: str = data.formatted[TRACEBUFFER_INFO_COUNT]
        self.assertTrue(msg.startswith("a simple example with some args "))
        file: str = data.file[TRACEBUFFER_INFO_COUNT]
        self.assertTrue(file.endswith("simple_cpp.cpp"))

    pass

class complex_c(unittest):
    target = "example-complex_c"
    root = pathlib.Path().resolve()

    def test_FORMAT_TEST(self: TestCase):
        data: pd.DataFrame = self.decoded
        format_test = data[data.tracebuffer == "FORMAT_TEST"]

        for row in format_test.itertuples():
            test_data = json.loads(row.formatted)
            if "expected" in test_data:
                self.assertEqual(test_data["expected"], test_data["got"])
            pass
        pass
    
    def test_DESTRUCTOR(self: TestCase):
        data: pd.DataFrame = self.decoded
        DESTRUCTOR = data[data.tracebuffer == "DESTRUCTOR"]
        formatted = DESTRUCTOR['formatted'].tolist()
        self.assertIn("destructor101", formatted)
        self.assertIn("destructor102", formatted)
        self.assertIn("destructor103", formatted)
        self.assertEqual(3 + TRACEBUFFER_INFO_COUNT, len(DESTRUCTOR))
        pass 
    pass


class complex_cpp(unittest):
    target = "example-complex_cpp"
    root = pathlib.Path().resolve()

    def test_FORMAT_TEST(self: TestCase):
        data: pd.DataFrame = self.decoded
        format_test = data[data.tracebuffer == "FORMAT_TEST"]

        for row in format_test.itertuples():
            test_data = json.loads(row.formatted)
            if "expected" in test_data:
                self.assertEqual(test_data["expected"], test_data["got"])
            pass
            pass
        pass
    
    def test_macro_as_tracebuffer_name(self: TestCase):
        data: pd.DataFrame = self.decoded
        
        COMPLEX_CPP_A = data[data.tracebuffer == "COMPLEX_CPP_A"]
        self.assertEqual(len(COMPLEX_CPP_A), 3 + TRACEBUFFER_INFO_COUNT)

        COMPLEX_CPP_B = data[data.tracebuffer == "COMPLEX_CPP_B"]
        self.assertEqual(len(COMPLEX_CPP_B), 1 + TRACEBUFFER_INFO_COUNT)
    pass

    def test_Tracebuffer_placement(self: TestCase):
        data: pd.DataFrame = self.decoded

        for row in data.itertuples():
            try:
                test_data = json.loads(row.formatted)
                if "tracebuffer" in test_data:
                    self.assertEqual(row["tracebuffer"], test_data["tracebuffer"])
            except:
                pass
            pass
        pass
    
    def test_DESTRUCTOR(self: TestCase):
        data: pd.DataFrame = self.decoded
        DESTRUCTOR = data[data.tracebuffer == "DESTRUCTOR_CPP"]
        formatted = DESTRUCTOR['formatted'].tolist()
        self.assertIn("void destructor101()", formatted)
        self.assertIn("void destructor102()", formatted)
        self.assertIn("void destructor103()", formatted)
        self.assertIn("TestClass::TestClass()", formatted)
        self.assertIn("TestClass::~TestClass()", formatted)
        self.assertEqual(5 + TRACEBUFFER_INFO_COUNT, len(DESTRUCTOR))
        pass 
    pass
    
    def test_TEMPLATE(self: TestCase):
        data: pd.DataFrame = self.decoded
        TEMPLATE = data[data.tracebuffer == "TEMPLATE"]
        TEMPLATE = TEMPLATE[ ~ TEMPLATE['formatted'].str.contains(r'tracebuffer info')]
        TEMPLATE['type'] = TEMPLATE['formatted'].str.extract(r'(?<=\[with Type = )(.*)(?=\])')
        grouped = TEMPLATE.groupby('type')
        for group_name, group_df in grouped:
            self.assertEqual(len(group_df), 3)
            self.assertIn(group_name, ('double','char','int','bool'))
    pass

class format_c(unittest):
    target = "example-gen_format_c"
    root = pathlib.Path().resolve()

    def test(self: unittest):
        data: pd.DataFrame = self.decoded
        self.assertEqual(len(data), 396 + TRACEBUFFER_INFO_COUNT)
        format_c = data[data["tracebuffer"] == "GEN_FORMAT_C"]
        self.assertEqual(len(format_c), 396 + TRACEBUFFER_INFO_COUNT)
    pass

class with_libraries(unittest):
    target = "example-with_libraries"
    root = pathlib.Path().resolve()

    def test(self):
        data: pd.DataFrame = self.decoded
        tb_names = data.tracebuffer.unique()

        self.assertIn("with_libraries", tb_names)
        with_libraries = data[
            data.tracebuffer == "with_libraries"]
        self.assertEqual(len(with_libraries), 12 + TRACEBUFFER_INFO_COUNT,
                         f'{len(with_libraries) = }')

        self.assertIn("with_libraries_main", tb_names)
        with_libraries_main = data[
            data.tracebuffer == "with_libraries_main"]
        self.assertEqual(len(with_libraries_main), 3 + TRACEBUFFER_INFO_COUNT,
                         f'{len(with_libraries_main) = }')

        self.assertIn("with_libraries_static", tb_names)
        with_libraries_static = data[
            data.tracebuffer == "with_libraries_static"]
        self.assertEqual(len(with_libraries_static), 3 + TRACEBUFFER_INFO_COUNT,
                         f'{len(with_libraries_static) = }')

        self.assertIn("with_libraries_shared", tb_names)
        with_libraries_shared = data[
            data.tracebuffer == "with_libraries_shared"]
        self.assertEqual(len(with_libraries_shared), 3 + TRACEBUFFER_INFO_COUNT,
                         f'{len(with_libraries_shared) = }')

        self.assertIn("with_libraries_dynamic", tb_names)
        with_libraries_dynamic = data[
            data.tracebuffer == "with_libraries_dynamic"]
        self.assertEqual(len(with_libraries_dynamic), 3 + TRACEBUFFER_INFO_COUNT,
                         f'{len(with_libraries_dynamic) = }')
        pass

    pass


class process_threads(unittest):
    target = "example-process_threads"
    root = pathlib.Path().resolve()
    regex: re.Pattern = re.compile(r'pid=(?P<pid>[0-9]+) tid=(?P<tid>[0-9]+)')

    def test(self):
        data: pd.DataFrame = self.decoded
        self.assertEqual(len(data), 2200 + TRACEBUFFER_INFO_COUNT)

        self.assertEqual(len(data.pid.unique()), 3)
        for pid in data.pid.unique():
            l = len(data[data.pid == pid])
            self.assertIn(l, [TRACEBUFFER_INFO_COUNT, 1100])

        self.assertEqual(len(data.tid.unique()), 23)
        for tid in data.tid.unique():
            l = len(data[data.tid == tid])
            self.assertIn(l, [TRACEBUFFER_INFO_COUNT, 100])

        for row in data.itertuples():
            match = self.regex.match(row.formatted)
            if match is not None:
                match = match.groupdict()
                pid, tid = (int(match['pid']), int(match['tid']))
                self.assertEqual(pid, row.pid)
                self.assertEqual(tid, row.tid)
            pass
        pass


if __name__ == '__main__':
    unittest.main()
