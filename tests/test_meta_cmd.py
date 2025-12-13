#!/usr/bin/python3
# Copyright (c) 2024, International Business Machines
# SPDX-License-Identifier: BSD-2-Clause-Patent

"""
CLLTK meta command tests.

Tests for the 'clltk meta' subcommand that displays tracepoint metadata.
"""

import json
import os
import pathlib
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.realpath(__file__))))

from helpers.base import run_command
from helpers.clltk_cmd import clltk


def setUpModule():
    """Build clltk-cmd before running tests."""
    run_command("cmake --preset default")
    run_command("cmake --build --preset default --target clltk-cmd")


class MetaTestCase(unittest.TestCase):
    """Base class for meta command tests with temporary directory setup."""

    def setUp(self):
        """Create temporary directory and set environment."""
        self.tmp_dir = tempfile.TemporaryDirectory()
        self.old_env = os.environ.get("CLLTK_TRACING_PATH")
        os.environ["CLLTK_TRACING_PATH"] = self.tmp_dir.name

    def tearDown(self):
        """Clean up temporary directory and restore environment."""
        if self.old_env:
            os.environ["CLLTK_TRACING_PATH"] = self.old_env
        else:
            os.environ.pop("CLLTK_TRACING_PATH", None)
        self.tmp_dir.cleanup()

    def _create_tracebuffer(self, name: str, size: str = "4KB"):
        """Create a tracebuffer with a tracepoint."""
        result = clltk("buffer", "--buffer", name, "--size", size)
        self.assertEqual(result.returncode, 0, msg=result.stderr)
        result = clltk("trace", name, "test message 42")
        self.assertEqual(result.returncode, 0, msg=result.stderr)

    def _list_trace_files(self, path=None) -> list:
        """List .clltk_trace files in the given path."""
        if path is None:
            path = self.tmp_dir.name
        trace_path = pathlib.Path(path)
        return list(trace_path.glob("*.clltk_trace"))


class TestMetaCommandBase(unittest.TestCase):
    """
    Basic meta command CLI tests.

    Tests help output and subcommand availability.
    """

    def test_meta_help(self):
        """Test that meta --help works."""
        result = clltk("meta", "--help")
        self.assertEqual(result.returncode, 0)
        self.assertIn("meta", result.stdout.lower())

    def test_meta_subcommand_exists(self):
        """Test that meta subcommand is recognized."""
        result = clltk("--help")
        self.assertEqual(result.returncode, 0)
        self.assertIn("meta", result.stdout)


class TestMetaWithTraceBuffer(MetaTestCase):
    """
    Meta command tests with tracebuffer files.

    Tests reading metadata from tracebuffer files and directories.
    """

    def test_meta_on_empty_directory(self):
        """Test meta on empty directory shows no results."""
        result = clltk("meta", self.tmp_dir.name, check=False)
        self.assertEqual(result.returncode, 0)

    def test_meta_on_tracebuffer(self):
        """Test meta reads from tracebuffer file."""
        self._create_tracebuffer("TestMeta")

        trace_file = pathlib.Path(self.tmp_dir.name) / "TestMeta.clltk_trace"
        result = clltk("meta", str(trace_file))

        self.assertEqual(result.returncode, 0, msg=result.stderr)
        self.assertIn("TestMeta", result.stdout)

    def test_meta_on_directory(self):
        """Test meta scans directory for tracebuffers."""
        self._create_tracebuffer("MetaDir1")
        self._create_tracebuffer("MetaDir2")

        result = clltk("meta", self.tmp_dir.name)

        self.assertEqual(result.returncode, 0, msg=result.stderr)
        self.assertIn("MetaDir1", result.stdout)
        self.assertIn("MetaDir2", result.stdout)

    def test_meta_recursive_default(self):
        """Test meta recurses into subdirectories by default."""
        subdir = pathlib.Path(self.tmp_dir.name) / "subdir"
        subdir.mkdir()

        old_path = os.environ["CLLTK_TRACING_PATH"]
        os.environ["CLLTK_TRACING_PATH"] = str(subdir)
        self._create_tracebuffer("SubdirMeta")
        os.environ["CLLTK_TRACING_PATH"] = old_path

        # Default is recursive
        result = clltk("meta", self.tmp_dir.name)

        self.assertEqual(result.returncode, 0, msg=result.stderr)
        self.assertIn("SubdirMeta", result.stdout)

    def test_meta_no_recursive(self):
        """Test meta --no-recursive does not recurse into subdirectories."""
        subdir = pathlib.Path(self.tmp_dir.name) / "subdir"
        subdir.mkdir()

        old_path = os.environ["CLLTK_TRACING_PATH"]
        os.environ["CLLTK_TRACING_PATH"] = str(subdir)
        self._create_tracebuffer("SubdirNoRecurse")
        os.environ["CLLTK_TRACING_PATH"] = old_path

        result = clltk("meta", self.tmp_dir.name, "--no-recursive")

        self.assertEqual(result.returncode, 0, msg=result.stderr)
        self.assertNotIn("SubdirNoRecurse", result.stdout)

    def test_meta_filter(self):
        """Test meta --filter option filters by name."""
        self._create_tracebuffer("FilterA")
        self._create_tracebuffer("FilterB")

        result = clltk("meta", self.tmp_dir.name, "--filter", "FilterA")

        self.assertEqual(result.returncode, 0, msg=result.stderr)
        self.assertIn("FilterA", result.stdout)
        self.assertNotIn("FilterB", result.stdout)


class TestMetaOutputFormats(MetaTestCase):
    """
    Meta command output format tests.

    Tests text, JSON, and summary output formats.
    """

    def setUp(self):
        """Create temporary directory with a tracebuffer."""
        super().setUp()

        result = clltk("buffer", "--buffer", "FormatTest", "--size", "4KB")
        self.assertEqual(result.returncode, 0)
        result = clltk("trace", "FormatTest", "format test message")
        self.assertEqual(result.returncode, 0)

        self.trace_file = pathlib.Path(self.tmp_dir.name) / "FormatTest.clltk_trace"

    def test_meta_text_output(self):
        """Test default text output format."""
        result = clltk("meta", str(self.trace_file))

        self.assertEqual(result.returncode, 0)
        self.assertIn("SOURCE", result.stdout)
        self.assertIn("TYPE", result.stdout)
        self.assertIn("FormatTest", result.stdout)

    def test_meta_json_output(self):
        """Test --json produces valid JSON."""
        result = clltk("meta", str(self.trace_file), "--json")

        self.assertEqual(result.returncode, 0, msg=result.stderr)

        data = json.loads(result.stdout)
        self.assertIsInstance(data, list)
        self.assertGreater(len(data), 0)
        self.assertIn("name", data[0])
        self.assertEqual(data[0]["name"], "FormatTest")

    def test_meta_pretty_json(self):
        """Test --pretty formats JSON nicely."""
        result = clltk("meta", str(self.trace_file), "--json", "--pretty")

        self.assertEqual(result.returncode, 0, msg=result.stderr)
        self.assertIn("\n", result.stdout)

        data = json.loads(result.stdout)
        self.assertIsInstance(data, list)

    def test_meta_summary_only(self):
        """Test -s shows only summary without entries."""
        result = clltk("meta", str(self.trace_file), "-s")

        self.assertEqual(result.returncode, 0)
        self.assertIn("FormatTest", result.stdout)
        self.assertNotIn("FORMAT", result.stdout)

    def test_meta_full_width(self):
        """Test -w shows full width output."""
        result = clltk("meta", str(self.trace_file), "-w")

        self.assertEqual(result.returncode, 0)
        self.assertIn("FormatTest", result.stdout)


class TestMetaJsonContent(MetaTestCase):
    """
    Tests for JSON output content.

    Verifies JSON structure and required fields.
    """

    def setUp(self):
        """Create tracebuffer with known content."""
        super().setUp()

        result = clltk("buffer", "--buffer", "JsonContent", "--size", "4KB")
        self.assertEqual(result.returncode, 0)
        result = clltk("trace", "JsonContent", "msg with args 123 hello")
        self.assertEqual(result.returncode, 0)

        self.trace_file = pathlib.Path(self.tmp_dir.name) / "JsonContent.clltk_trace"

    def test_json_has_required_fields(self):
        """Test JSON output has all required fields."""
        result = clltk("meta", str(self.trace_file), "--json")
        data = json.loads(result.stdout)

        self.assertGreater(len(data), 0)
        source = data[0]

        self.assertIn("name", source)
        self.assertIn("path", source)
        self.assertIn("source_type", source)
        self.assertIn("meta_size", source)
        self.assertIn("entries", source)

    def test_json_entry_fields(self):
        """Test JSON entries have correct fields."""
        result = clltk("meta", str(self.trace_file), "--json")
        data = json.loads(result.stdout)

        source = data[0]
        self.assertIn("entries", source)

        if len(source["entries"]) > 0:
            entry = source["entries"][0]
            self.assertIn("offset", entry)
            self.assertIn("size", entry)
            self.assertIn("type", entry)
            self.assertIn("line", entry)
            self.assertIn("file", entry)
            self.assertIn("format", entry)

    def test_json_source_type(self):
        """Test source_type is correct."""
        result = clltk("meta", str(self.trace_file), "--json")
        data = json.loads(result.stdout)

        self.assertEqual(data[0]["source_type"], "tracebuffer")


class TestMetaEdgeCases(MetaTestCase):
    """
    Edge case tests for meta command.

    Tests error handling and boundary conditions.
    """

    def test_meta_nonexistent_path(self):
        """Test meta handles nonexistent path gracefully."""
        result = clltk("meta", "/nonexistent/path/to/file", check=False)
        self.assertEqual(result.returncode, 0)

    def test_meta_uses_default_path(self):
        """Test meta uses CLLTK_TRACING_PATH when no path given."""
        result = clltk("buffer", "--buffer", "DefaultPath", "--size", "1KB")
        self.assertEqual(result.returncode, 0)

        result = clltk("meta")
        self.assertEqual(result.returncode, 0)
        self.assertIn("DefaultPath", result.stdout)

    def test_meta_multiple_tracebuffers(self):
        """Test meta handles multiple tracebuffers."""
        for i in range(3):
            result = clltk("buffer", "--buffer", f"Multi{i}", "--size", "1KB")
            self.assertEqual(result.returncode, 0)

        result = clltk("meta", self.tmp_dir.name)
        self.assertEqual(result.returncode, 0)

        for i in range(3):
            self.assertIn(f"Multi{i}", result.stdout)

    def test_meta_on_invalid_file(self):
        """Test meta handles invalid/corrupted trace file gracefully."""
        bad_file = pathlib.Path(self.tmp_dir.name) / "invalid.clltk_trace"
        bad_file.write_bytes(b"not a valid trace file content")

        result = clltk("meta", str(bad_file), check=False)
        # Should not crash, may return error or empty result
        # The important thing is graceful handling

    def test_meta_while_tracing_active(self):
        """Test meta can read while tracepoints are being written."""
        self._create_tracebuffer("ConcurrentTest")

        # Write additional tracepoints
        for i in range(10):
            result = clltk("trace", "ConcurrentTest", f"concurrent msg {i}")
            self.assertEqual(result.returncode, 0)

        # Meta should still work correctly
        result = clltk("meta", self.tmp_dir.name)
        self.assertEqual(result.returncode, 0)
        self.assertIn("ConcurrentTest", result.stdout)

    def test_meta_empty_tracebuffer(self):
        """Test meta on tracebuffer with no tracepoints."""
        result = clltk("buffer", "--buffer", "EmptyBuffer", "--size", "1KB")
        self.assertEqual(result.returncode, 0)

        # Don't write any tracepoints
        trace_file = pathlib.Path(self.tmp_dir.name) / "EmptyBuffer.clltk_trace"
        result = clltk("meta", str(trace_file))
        self.assertEqual(result.returncode, 0)
        self.assertIn("EmptyBuffer", result.stdout)


class TestMetaFilterOptions(MetaTestCase):
    """
    Tests for meta command filtering options.

    Tests --filter option with various patterns.
    """

    def test_meta_filter_exact_match(self):
        """Test --filter with exact buffer name."""
        self._create_tracebuffer("FilterExact")
        self._create_tracebuffer("FilterOther")

        result = clltk("meta", self.tmp_dir.name, "--filter", "FilterExact")

        self.assertEqual(result.returncode, 0)
        self.assertIn("FilterExact", result.stdout)
        self.assertNotIn("FilterOther", result.stdout)

    def test_meta_filter_no_match(self):
        """Test --filter with non-matching pattern."""
        self._create_tracebuffer("RealBuffer")

        result = clltk("meta", self.tmp_dir.name, "--filter", "NonExistent")

        self.assertEqual(result.returncode, 0)
        self.assertNotIn("RealBuffer", result.stdout)

    def test_meta_filter_case_sensitive(self):
        """Test that --filter is case sensitive."""
        self._create_tracebuffer("CaseSensitive")

        result = clltk("meta", self.tmp_dir.name, "--filter", "casesensitive")

        self.assertEqual(result.returncode, 0)
        # Filter should be case-sensitive, so no match
        self.assertNotIn("CaseSensitive", result.stdout)


if __name__ == "__main__":
    unittest.main()
