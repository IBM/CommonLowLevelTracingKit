#!/usr/bin/python3
# Copyright (c) 2026, International Business Machines
# SPDX-License-Identifier: BSD-2-Clause-Patent

"""
Golden file tests: decode committed trace file fixtures and compare against
the known content. The fixtures cover different library versions and both
byte orders; see tests/golden/README.md.
"""

import csv
import os
import pathlib
import subprocess
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.dirname(os.path.realpath(__file__)))

GOLDEN_DIR = pathlib.Path(__file__).parent / "golden"
PYTHON_DECODER = pathlib.Path(__file__).parent.parent / "decoder_tool" / "python" / "clltk_decoder.py"

# formatted messages written by tests/golden/generator/writer.c
EXPECTED_MESSAGES = [
    "plain int 42",
    "u8 8 u16 1616 u32 323232",
    "i64 -64646464 u64 18446744073709551615",
    "float 3.500000 double -2.250000",
    "string golden string",
    "pointer 0x123456789abc",
    'golden dump =(dump)= "DE AD BE EF 01 02 03 04"',
]

LITTLE_ENDIAN_FIXTURES = [
    "golden-1.2.64-le-aarch64.clltk_trace",
    "golden-1.3.0-le-aarch64.clltk_trace",
]
BIG_ENDIAN_FIXTURES = [
    "golden-1.3.0-be-s390x.clltk_trace",
]


def decode_with_python(fixture: pathlib.Path) -> list:
    """Decode one fixture with the python decoder, return formatted messages."""
    with tempfile.TemporaryDirectory() as tmp:
        out_csv = pathlib.Path(tmp) / "out.csv"
        result = subprocess.run(
            [sys.executable, str(PYTHON_DECODER), "-o", str(out_csv), str(fixture)],
            capture_output=True,
            text=True,
        )
        assert result.returncode == 0, result.stderr
        with open(out_csv, newline="") as fh:
            rows = list(csv.DictReader(fh))
    messages = [row["formatted"].strip() for row in rows]
    return [m for m in messages if not m.startswith('{"tracebuffer info')]


class golden_python_decoder(unittest.TestCase):
    def test_little_endian_fixtures(self):
        for name in LITTLE_ENDIAN_FIXTURES:
            with self.subTest(fixture=name):
                messages = decode_with_python(GOLDEN_DIR / name)
                self.assertEqual(EXPECTED_MESSAGES, messages)

    def test_big_endian_fixtures(self):
        for name in BIG_ENDIAN_FIXTURES:
            with self.subTest(fixture=name):
                messages = decode_with_python(GOLDEN_DIR / name)
                self.assertEqual(EXPECTED_MESSAGES, messages)


if __name__ == "__main__":
    unittest.main()
