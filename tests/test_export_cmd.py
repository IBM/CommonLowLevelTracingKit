#!/usr/bin/python3
# Copyright (c) 2026, International Business Machines
# SPDX-License-Identifier: BSD-2-Clause-Patent

"""
Tests for the 'clltk export' subcommand: Chrome/Perfetto trace event JSON
generated from the golden fixtures.
"""

import json
import os
import pathlib
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.dirname(os.path.realpath(__file__)))

from helpers.base import run_command
from helpers.clltk_cmd import clltk

GOLDEN_DIR = pathlib.Path(__file__).parent / "golden"


def setUpModule():
    """Build clltk-cmd before running tests."""
    run_command("cmake --preset default")
    run_command("cmake --build --preset default --target clltk-cmd")


def export_fixture(name: str) -> dict:
    with tempfile.TemporaryDirectory() as tmp:
        out = pathlib.Path(tmp) / "out.json"
        result = clltk("export", str(GOLDEN_DIR / name), "-o", str(out))
        assert result.returncode == 0, result.stderr
        with open(out) as fh:
            return json.load(fh)


class export_golden_fixtures(unittest.TestCase):
    def test_span_fixture_little_endian(self):
        self._check_span_fixture("1.5.0/le-aarch64.clltk_trace")

    def test_span_fixture_big_endian(self):
        self._check_span_fixture("1.5.0/be-s390x.clltk_trace")

    def _check_span_fixture(self, name: str):
        data = export_fixture(name)
        events = data["traceEvents"]

        begins = [e for e in events if e["ph"] == "b"]
        ends = [e for e in events if e["ph"] == "e"]
        instants = [e for e in events if e["ph"] == "i"]

        # golden writer: 3 span begins (one never ends), 2 ends, 7 tracepoints
        self.assertEqual(3, len(begins))
        self.assertEqual(2, len(ends))
        self.assertEqual(7, len(instants))

        # every end pairs with a begin; the open span has no end
        begin_ids = {e["id"] for e in begins}
        end_ids = {e["id"] for e in ends}
        self.assertTrue(end_ids.issubset(begin_ids))
        self.assertEqual(1, len(begin_ids - end_ids))

        # the inner span carries its parent's id
        by_name = {e["name"]: e for e in begins}
        self.assertIn("golden inner span", by_name)
        self.assertEqual(
            by_name["golden inner span"]["args"]["parent"],
            by_name["golden outer span"]["id"],
        )

        # timestamps ascend and instants carry source location
        timestamps = [e["ts"] for e in events]
        self.assertEqual(timestamps, sorted(timestamps))
        for event in instants:
            self.assertIn("file", event["args"])
            self.assertIn("line", event["args"])

    def test_fixture_without_spans(self):
        data = export_fixture("1.3.0/le-aarch64.clltk_trace")
        events = data["traceEvents"]
        self.assertEqual(7, len([e for e in events if e["ph"] == "i"]))
        self.assertEqual(0, len([e for e in events if e["ph"] in ("b", "e")]))


if __name__ == "__main__":
    unittest.main()
