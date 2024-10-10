#!/usr/bin/env python3
# Copyright (c) 2024, International Business Machines
# SPDX-License-Identifier: BSD-2-Clause-Patent

import os
script_dir = os.path.dirname(os.path.realpath(__file__))
root = os.path.join(script_dir, "..", "..")
root = os.path.abspath(root)
decoder_dir = os.path.join(root, "decoder_tool", "python")
assert os.path.isdir(decoder_dir), f'missing decoder folder in {decoder_dir}'
import sys
sys.path.append(decoder_dir)

import clltk_decoder
import unittest

class clltk_decoder_decode_tests(unittest.TestCase):

    def test_decoder(self:unittest.TestCase):
        self.assertEqual("", clltk_decoder.decode(b""))
        self.assertEqual("\0", clltk_decoder.decode(b"\0"))
        self.assertEqual("\\n", clltk_decoder.decode(b"\n"))
        self.assertEqual("\\t", clltk_decoder.decode(b"\t"))
        self.assertEqual("A\0B", clltk_decoder.decode(b"A\0B"))
        self.assertEqual("ABC\0", clltk_decoder.decode(b"ABC\0"))
        
        pass