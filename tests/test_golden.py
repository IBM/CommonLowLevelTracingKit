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
    "golden-1.5.0-le-aarch64.clltk_trace",  # first version with span events
]
BIG_ENDIAN_FIXTURES = [
    "golden-1.3.0-be-s390x.clltk_trace",
    "golden-1.5.0-be-s390x.clltk_trace",
]
# fixtures containing span events (validated structurally: span ids are
# random per generation, so exact strings differ between fixtures)
SPAN_FIXTURES = [
    "golden-1.5.0-le-aarch64.clltk_trace",
    "golden-1.5.0-be-s390x.clltk_trace",
]


def split_span_rows(messages: list) -> tuple:
    """Separate span begin/end rows from regular tracepoint rows."""
    spans = [m for m in messages if m.startswith(">>>") or m.startswith("<<<")]
    regular = [m for m in messages if m not in spans]
    return regular, spans


def validate_span_structure(test: unittest.TestCase, span_messages: list):
    """The golden writer creates: outer span with an inner child (both
    ended) and one span that never ends. Span ids are random per
    generation, so validate the structure instead of exact strings."""
    import re

    begins = {}
    ends = []
    for message in span_messages:
        m = re.match(r">>> (.+) \[span (0x[0-9a-f]+)(?: parent (0x[0-9a-f]+))?\]", message)
        if m:
            begins[m.group(1)] = (m.group(2), m.group(3))
            continue
        m = re.match(r"<<< \[span (0x[0-9a-f]+)\]", message)
        if m:
            ends.append(m.group(1))
    test.assertEqual(
        {"golden outer span", "golden inner span", "golden open span"}, set(begins)
    )
    test.assertEqual(begins["golden inner span"][1], begins["golden outer span"][0])
    test.assertIsNone(begins["golden outer span"][1])
    test.assertIsNone(begins["golden open span"][1])
    test.assertEqual(
        sorted(ends),
        sorted([begins["golden outer span"][0], begins["golden inner span"][0]]),
    )


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


def assert_fixture(test: unittest.TestCase, name: str, messages: list):
    """Regular rows must match exactly; span rows are validated structurally."""
    regular, spans = split_span_rows(messages)
    test.assertEqual(EXPECTED_MESSAGES, regular)
    if name in SPAN_FIXTURES:
        validate_span_structure(test, spans)
    else:
        test.assertEqual([], spans)


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
        if formatted.startswith("!timestamp") or formatted == "formatted":
            continue
        messages.append(formatted)
    return messages


class golden_python_decoder(unittest.TestCase):
    def test_little_endian_fixtures(self):
        for name in LITTLE_ENDIAN_FIXTURES:
            with self.subTest(fixture=name):
                messages = decode_with_python(GOLDEN_DIR / name)
                assert_fixture(self, name, messages)

    def test_big_endian_fixtures(self):
        for name in BIG_ENDIAN_FIXTURES:
            with self.subTest(fixture=name):
                messages = decode_with_python(GOLDEN_DIR / name)
                assert_fixture(self, name, messages)


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
                assert_fixture(self, name, messages)

    def test_big_endian_fixtures(self):
        for name in BIG_ENDIAN_FIXTURES:
            with self.subTest(fixture=name):
                messages = decode_with_cli(GOLDEN_DIR / name)
                assert_fixture(self, name, messages)


if __name__ == "__main__":
    unittest.main()
