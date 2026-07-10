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

from helpers.base import run_command
from helpers.clltk_cmd import clltk


def setUpModule():
    """Build clltk-cmd before running the CLI golden tests."""
    run_command("cmake --preset default")
    run_command("cmake --build --preset default --target clltk-cmd")

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
    "golden-1.2.39-le-aarch64.clltk_trace",  # oldest buildable, pre dump-format change (1.2.40)
    "golden-1.2.49-le-aarch64.clltk_trace",  # pre definition-V2 format (1.2.50)
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


def decode_with_cli(fixture: pathlib.Path) -> list:
    """Decode one fixture with the clltk CLI, return formatted messages."""
    result = clltk("decode", str(fixture))
    assert result.returncode == 0, result.stderr
    messages = []
    for line in result.stdout.splitlines():
        # column layout: timestamp | time | tracebuffer | pid | tid | formatted | file | line
        columns = line.split("|")
        if len(columns) < 8:
            continue
        formatted = columns[5].strip()
        if formatted in EXPECTED_MESSAGES:
            messages.append(formatted)
    return messages


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


class golden_elf_meta(unittest.TestCase):
    """clltk meta extracts tracepoint metadata from committed big-endian
    s390x ELF objects: the .so through virtual addresses, the .o through
    relocation records."""

    def _assert_meta(self, name: str):
        result = clltk("meta", str(GOLDEN_DIR / name))
        self.assertEqual(result.returncode, 0, msg=result.stderr)
        self.assertIn("BE_ELF_TEST", result.stdout)
        self.assertIn("big endian elf tracepoint %d", result.stdout)

    def test_big_endian_shared_object(self):
        self._assert_meta("golden-1.3.0-be-s390x.so")

    def test_big_endian_relocatable_object(self):
        self._assert_meta("golden-1.3.0-be-s390x.o")


class golden_cli_decoder(unittest.TestCase):
    def test_little_endian_fixtures(self):
        for name in LITTLE_ENDIAN_FIXTURES:
            with self.subTest(fixture=name):
                messages = decode_with_cli(GOLDEN_DIR / name)
                self.assertEqual(EXPECTED_MESSAGES, messages)

    def test_big_endian_fixtures(self):
        for name in BIG_ENDIAN_FIXTURES:
            with self.subTest(fixture=name):
                messages = decode_with_cli(GOLDEN_DIR / name)
                self.assertEqual(EXPECTED_MESSAGES, messages)


if __name__ == "__main__":
    unittest.main()
