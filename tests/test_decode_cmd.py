#!/usr/bin/python3
# Copyright (c) 2024, International Business Machines
# SPDX-License-Identifier: BSD-2-Clause-Patent

"""
CLLTK decode command tests.

Tests for the 'clltk decode' subcommand that decodes and formats trace files.
"""

import gzip
import json
import os
import pathlib
import re
import sys
import tempfile
import time
import unittest

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.realpath(__file__))))

from helpers.base import run_command
from helpers.clltk_cmd import clltk


def setUpModule():
    """Build clltk-cmd before running tests."""
    run_command("cmake --preset default")
    run_command("cmake --build --preset default --target clltk-cmd")


class DecodeTestCase(unittest.TestCase):
    """Base class for decode command tests with temporary directory setup."""

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

    def _create_tracebuffer_with_messages(
        self, name: str, messages: list, size: str = "4KB"
    ):
        """Create a tracebuffer with multiple tracepoints."""
        result = clltk("buffer", "--buffer", name, "--size", size)
        self.assertEqual(result.returncode, 0, msg=result.stderr)
        for msg in messages:
            result = clltk("trace", name, msg)
            self.assertEqual(result.returncode, 0, msg=result.stderr)
            time.sleep(0.01)  # Small delay to ensure different timestamps

    def _list_trace_files(self, path=None) -> list:
        """List .clltk_trace files in the given path."""
        if path is None:
            path = self.tmp_dir.name
        trace_path = pathlib.Path(path)
        return list(trace_path.glob("*.clltk_trace"))


class TestDecodeCommandBase(unittest.TestCase):
    """
    Basic decode command CLI tests.

    Tests help output and subcommand availability.
    """

    def test_decode_help(self):
        """Test that decode --help works."""
        result = clltk("decode", "--help")
        self.assertEqual(result.returncode, 0)
        self.assertIn("decode", result.stdout.lower())

    def test_decode_subcommand_exists(self):
        """Test that decode subcommand is recognized."""
        result = clltk("--help")
        self.assertEqual(result.returncode, 0)
        self.assertIn("decode", result.stdout)

    def test_decode_help_shows_options(self):
        """Test that help shows all expected options."""
        result = clltk("decode", "--help")
        self.assertEqual(result.returncode, 0)
        self.assertIn("--json", result.stdout)
        self.assertIn("--output", result.stdout)
        self.assertIn("--filter", result.stdout)
        self.assertIn("--pid", result.stdout)
        self.assertIn("--tid", result.stdout)
        self.assertIn("--msg", result.stdout)
        self.assertIn("--msg-regex", result.stdout)
        self.assertIn("--sorted", result.stdout)
        self.assertIn("--unsorted", result.stdout)
        self.assertIn("--since", result.stdout)
        self.assertIn("--until", result.stdout)
        self.assertIn("--source", result.stdout)


class TestDecodeBasicFunctionality(DecodeTestCase):
    """
    Basic decode command functionality tests.

    Tests decoding single files, directories, and output to file.
    """

    def test_decode_single_file(self):
        """Test decoding a single tracebuffer file."""
        self._create_tracebuffer("SingleFile")

        trace_file = pathlib.Path(self.tmp_dir.name) / "SingleFile.clltk_trace"
        result = clltk("decode", str(trace_file))

        self.assertEqual(result.returncode, 0, msg=result.stderr)
        self.assertIn("SingleFile", result.stdout)
        self.assertIn("test message 42", result.stdout)

    def test_decode_directory(self):
        """Test decoding all tracebuffers in a directory."""
        self._create_tracebuffer("DirTest1")
        self._create_tracebuffer("DirTest2")

        result = clltk("decode", self.tmp_dir.name)

        self.assertEqual(result.returncode, 0, msg=result.stderr)
        self.assertIn("DirTest1", result.stdout)
        self.assertIn("DirTest2", result.stdout)

    def test_decode_output_to_file(self):
        """Test --output writes to file instead of stdout."""
        self._create_tracebuffer("OutputTest")

        output_file = pathlib.Path(self.tmp_dir.name) / "output.txt"
        result = clltk("decode", self.tmp_dir.name, "--output", str(output_file))

        self.assertEqual(result.returncode, 0, msg=result.stderr)
        self.assertTrue(output_file.exists())

        content = output_file.read_text()
        self.assertIn("OutputTest", content)
        self.assertIn("test message 42", content)

    def test_decode_output_dash_means_stdout(self):
        """Test --output - outputs to stdout."""
        self._create_tracebuffer("StdoutTest")

        result = clltk("decode", self.tmp_dir.name, "--output", "-")

        self.assertEqual(result.returncode, 0, msg=result.stderr)
        self.assertIn("StdoutTest", result.stdout)

    def test_decode_uses_default_path(self):
        """Test decode uses CLLTK_TRACING_PATH when no path given."""
        self._create_tracebuffer("DefaultPath")

        result = clltk("decode")

        self.assertEqual(result.returncode, 0, msg=result.stderr)
        self.assertIn("DefaultPath", result.stdout)

    def test_decode_recursive_default(self):
        """Test decode recurses into subdirectories by default."""
        subdir = pathlib.Path(self.tmp_dir.name) / "subdir"
        subdir.mkdir()

        old_path = os.environ["CLLTK_TRACING_PATH"]
        os.environ["CLLTK_TRACING_PATH"] = str(subdir)
        self._create_tracebuffer("SubdirTrace")
        os.environ["CLLTK_TRACING_PATH"] = old_path

        result = clltk("decode", self.tmp_dir.name)

        self.assertEqual(result.returncode, 0, msg=result.stderr)
        self.assertIn("SubdirTrace", result.stdout)

    def test_decode_no_recursive(self):
        """Test decode --no-recursive does not recurse into subdirectories."""
        subdir = pathlib.Path(self.tmp_dir.name) / "subdir"
        subdir.mkdir()

        old_path = os.environ["CLLTK_TRACING_PATH"]
        os.environ["CLLTK_TRACING_PATH"] = str(subdir)
        self._create_tracebuffer("SubdirNoRecurse")
        os.environ["CLLTK_TRACING_PATH"] = old_path

        result = clltk("decode", self.tmp_dir.name, "--no-recursive")

        self.assertEqual(result.returncode, 0, msg=result.stderr)
        self.assertNotIn("SubdirNoRecurse", result.stdout)


class TestDecodeJsonOutput(DecodeTestCase):
    """
    Decode command JSON output tests.

    Tests JSON format output and structure validation.
    """

    def setUp(self):
        """Create temporary directory with a tracebuffer."""
        super().setUp()

        result = clltk("buffer", "--buffer", "JsonTest", "--size", "4KB")
        self.assertEqual(result.returncode, 0)
        result = clltk("trace", "JsonTest", "json test message")
        self.assertEqual(result.returncode, 0)

        self.trace_file = pathlib.Path(self.tmp_dir.name) / "JsonTest.clltk_trace"

    def test_decode_json_output(self):
        """Test --json produces JSON output."""
        result = clltk("decode", str(self.trace_file), "--json")

        self.assertEqual(result.returncode, 0, msg=result.stderr)
        # JSON output is one object per line
        lines = [line for line in result.stdout.strip().split("\n") if line]
        self.assertGreater(len(lines), 0)

        # Each line should be valid JSON
        for line in lines:
            data = json.loads(line)
            self.assertIsInstance(data, dict)

    def test_decode_json_structure(self):
        """Test JSON output has all required fields."""
        result = clltk("decode", str(self.trace_file), "--json")

        self.assertEqual(result.returncode, 0, msg=result.stderr)

        lines = [line for line in result.stdout.strip().split("\n") if line]
        self.assertGreater(len(lines), 0)

        data = json.loads(lines[0])

        # Required fields from print_tracepoint_json
        self.assertIn("timestamp_ns", data)
        self.assertIn("timestamp", data)
        self.assertIn("datetime", data)
        self.assertIn("tracebuffer", data)
        self.assertIn("pid", data)
        self.assertIn("tid", data)
        self.assertIn("message", data)
        self.assertIn("file", data)
        self.assertIn("line", data)
        self.assertIn("is_kernel", data)
        self.assertIn("source_type", data)
        self.assertIn("tracepoint_nr", data)

    def test_decode_json_values(self):
        """Test JSON output contains correct values."""
        result = clltk("decode", str(self.trace_file), "--json")

        lines = [line for line in result.stdout.strip().split("\n") if line]
        data = json.loads(lines[0])

        self.assertEqual(data["tracebuffer"], "JsonTest")
        self.assertIn("json test message", data["message"])
        self.assertIsInstance(data["pid"], int)
        self.assertIsInstance(data["tid"], int)
        self.assertIsInstance(data["timestamp_ns"], int)
        self.assertIsInstance(data["is_kernel"], bool)

    def test_decode_json_multiple_tracepoints(self):
        """Test JSON output with multiple tracepoints."""
        messages = ["msg_a", "msg_b", "msg_c"]
        self._create_tracebuffer_with_messages("MultiJson", messages)

        trace_file = pathlib.Path(self.tmp_dir.name) / "MultiJson.clltk_trace"
        result = clltk("decode", str(trace_file), "--json")

        self.assertEqual(result.returncode, 0, msg=result.stderr)

        lines = [line for line in result.stdout.strip().split("\n") if line]
        self.assertGreaterEqual(len(lines), len(messages))

        parsed_messages = []
        for line in lines:
            data = json.loads(line)
            if data["tracebuffer"] == "MultiJson":
                parsed_messages.append(data["message"])

        for msg in messages:
            found = any(msg in parsed_msg for parsed_msg in parsed_messages)
            self.assertTrue(found, f"Message '{msg}' not found in JSON output")


class TestDecodeFiltering(DecodeTestCase):
    """
    Decode command filtering tests.

    Tests filtering by name, pid, tid, msg, and msg-regex.
    """

    def test_filter_by_name(self):
        """Test --filter filters by tracebuffer name regex."""
        self._create_tracebuffer("FilterA")
        self._create_tracebuffer("FilterB")
        self._create_tracebuffer("OtherBuffer")

        result = clltk("decode", self.tmp_dir.name, "--filter", "Filter.*")

        self.assertEqual(result.returncode, 0, msg=result.stderr)
        self.assertIn("FilterA", result.stdout)
        self.assertIn("FilterB", result.stdout)
        self.assertNotIn("OtherBuffer", result.stdout)

    def test_filter_exact_name(self):
        """Test --filter with exact buffer name."""
        self._create_tracebuffer("ExactMatch")
        self._create_tracebuffer("ExactMatchNot")

        result = clltk("decode", self.tmp_dir.name, "--filter", "^ExactMatch$")

        self.assertEqual(result.returncode, 0, msg=result.stderr)
        self.assertIn("ExactMatch", result.stdout)
        # Should not match ExactMatchNot with exact regex
        lines = result.stdout.split("\n")
        exact_match_lines = [
            l for l in lines if "ExactMatch" in l and "ExactMatchNot" not in l
        ]
        self.assertGreater(len(exact_match_lines), 0)

    def test_filter_by_pid(self):
        """Test --pid filters by process ID."""
        self._create_tracebuffer("PidTest")

        # Get current process ID
        current_pid = os.getpid()

        # First decode to get the actual PID from tracepoints
        result = clltk("decode", self.tmp_dir.name, "--json")
        self.assertEqual(result.returncode, 0, msg=result.stderr)

        lines = [line for line in result.stdout.strip().split("\n") if line]
        if lines:
            data = json.loads(lines[0])
            actual_pid = data["pid"]

            # Now filter by that PID
            result = clltk("decode", self.tmp_dir.name, "--pid", str(actual_pid))
            self.assertEqual(result.returncode, 0, msg=result.stderr)
            self.assertIn("PidTest", result.stdout)

            # Filter by non-existent PID
            result = clltk("decode", self.tmp_dir.name, "--pid", "999999")
            self.assertEqual(result.returncode, 0, msg=result.stderr)
            self.assertNotIn("test message 42", result.stdout)

    def test_filter_by_tid(self):
        """Test --tid filters by thread ID."""
        self._create_tracebuffer("TidTest")

        # Get TID from JSON output
        result = clltk("decode", self.tmp_dir.name, "--json")
        self.assertEqual(result.returncode, 0, msg=result.stderr)

        lines = [line for line in result.stdout.strip().split("\n") if line]
        if lines:
            data = json.loads(lines[0])
            actual_tid = data["tid"]

            # Filter by that TID
            result = clltk("decode", self.tmp_dir.name, "--tid", str(actual_tid))
            self.assertEqual(result.returncode, 0, msg=result.stderr)
            self.assertIn("TidTest", result.stdout)

            # Filter by non-existent TID
            result = clltk("decode", self.tmp_dir.name, "--tid", "999999")
            self.assertEqual(result.returncode, 0, msg=result.stderr)
            self.assertNotIn("test message 42", result.stdout)

    def test_filter_by_msg(self):
        """Test --msg filters by message substring."""
        self._create_tracebuffer_with_messages(
            "MsgFilter",
            ["apple banana cherry", "orange grape lemon", "apple orange pear"],
        )

        result = clltk("decode", self.tmp_dir.name, "--msg", "apple")

        self.assertEqual(result.returncode, 0, msg=result.stderr)
        self.assertIn("apple", result.stdout)
        # Should include messages with "apple"
        self.assertIn("banana", result.stdout)
        self.assertIn("pear", result.stdout)
        # Should not include "orange grape lemon" (no apple)
        self.assertNotIn("grape", result.stdout)

    def test_filter_by_msg_regex(self):
        """Test --msg-regex filters by message regex."""
        self._create_tracebuffer_with_messages(
            "MsgRegex",
            [
                "error: connection failed",
                "warning: low memory",
                "error: timeout exceeded",
                "info: operation complete",
            ],
        )

        result = clltk("decode", self.tmp_dir.name, "--msg-regex", "^error:")

        self.assertEqual(result.returncode, 0, msg=result.stderr)
        self.assertIn("error:", result.stdout)
        self.assertIn("connection failed", result.stdout)
        self.assertIn("timeout exceeded", result.stdout)
        self.assertNotIn("low memory", result.stdout)
        self.assertNotIn("operation complete", result.stdout)

    def test_filter_multiple_pids(self):
        """Test --pid can filter by multiple PIDs."""
        self._create_tracebuffer("MultiPid")

        # Get actual PID
        result = clltk("decode", self.tmp_dir.name, "--json")
        lines = [line for line in result.stdout.strip().split("\n") if line]
        if lines:
            data = json.loads(lines[0])
            actual_pid = data["pid"]

            # Filter by multiple PIDs (one real, one fake)
            result = clltk(
                "decode", self.tmp_dir.name, "--pid", str(actual_pid), "--pid", "999999"
            )
            self.assertEqual(result.returncode, 0, msg=result.stderr)
            self.assertIn("MultiPid", result.stdout)

    def test_filter_source_userspace(self):
        """Test --source userspace filters userspace traces only."""
        self._create_tracebuffer("UserspaceTest")

        result = clltk("decode", self.tmp_dir.name, "--source", "userspace")

        self.assertEqual(result.returncode, 0, msg=result.stderr)
        # Userspace traces created by clltk trace should be included
        self.assertIn("UserspaceTest", result.stdout)

    def test_filter_source_all(self):
        """Test --source all includes all traces."""
        self._create_tracebuffer("SourceAll")

        result = clltk("decode", self.tmp_dir.name, "--source", "all")

        self.assertEqual(result.returncode, 0, msg=result.stderr)
        self.assertIn("SourceAll", result.stdout)


class TestDecodeSorting(DecodeTestCase):
    """
    Decode command sorting tests.

    Tests sorted vs unsorted output ordering.
    """

    def test_sorted_output_default(self):
        """Test that output is sorted by timestamp by default."""
        # Create two buffers with interleaved timestamps
        result = clltk("buffer", "--buffer", "SortA", "--size", "4KB")
        self.assertEqual(result.returncode, 0)
        result = clltk("buffer", "--buffer", "SortB", "--size", "4KB")
        self.assertEqual(result.returncode, 0)

        # Write alternating messages
        for i in range(5):
            clltk("trace", "SortA", f"sortA_msg_{i}")
            time.sleep(0.01)
            clltk("trace", "SortB", f"sortB_msg_{i}")
            time.sleep(0.01)

        result = clltk("decode", self.tmp_dir.name, "--sorted", "--json")
        self.assertEqual(result.returncode, 0, msg=result.stderr)

        lines = [line for line in result.stdout.strip().split("\n") if line]
        timestamps = []
        for line in lines:
            data = json.loads(line)
            timestamps.append(data["timestamp_ns"])

        # Verify timestamps are in ascending order
        for i in range(1, len(timestamps)):
            self.assertLessEqual(
                timestamps[i - 1],
                timestamps[i],
                f"Timestamps not sorted: {timestamps[i - 1]} > {timestamps[i]}",
            )

    def test_sorted_flag_explicit(self):
        """Test --sorted flag explicitly enables sorting."""
        self._create_tracebuffer_with_messages("SortedTest", ["msg1", "msg2", "msg3"])

        result = clltk("decode", self.tmp_dir.name, "--sorted", "--json")
        self.assertEqual(result.returncode, 0, msg=result.stderr)

        lines = [line for line in result.stdout.strip().split("\n") if line]
        timestamps = [json.loads(line)["timestamp_ns"] for line in lines]

        # Should be sorted
        self.assertEqual(timestamps, sorted(timestamps))

    def test_unsorted_flag(self):
        """Test --unsorted flag disables global sorting."""
        # Create buffers
        result = clltk("buffer", "--buffer", "UnsortA", "--size", "4KB")
        self.assertEqual(result.returncode, 0)
        result = clltk("buffer", "--buffer", "UnsortB", "--size", "4KB")
        self.assertEqual(result.returncode, 0)

        # Write messages
        for i in range(3):
            clltk("trace", "UnsortA", f"unsortA_{i}")
            time.sleep(0.01)
            clltk("trace", "UnsortB", f"unsortB_{i}")
            time.sleep(0.01)

        result = clltk("decode", self.tmp_dir.name, "--unsorted")
        self.assertEqual(result.returncode, 0, msg=result.stderr)

        # Output should still contain all messages
        self.assertIn("UnsortA", result.stdout)
        self.assertIn("UnsortB", result.stdout)


class TestDecodeTimeFilters(DecodeTestCase):
    """
    Decode command time filter tests.

    Tests --since and --until with various formats.
    """

    def test_since_relative_format(self):
        """Test --since with relative time format (-5m style)."""
        self._create_tracebuffer("SinceRelative")

        # Filter from 1 hour ago - should include recent traces
        result = clltk("decode", self.tmp_dir.name, "--since", "-1h")
        self.assertEqual(result.returncode, 0, msg=result.stderr)
        self.assertIn("SinceRelative", result.stdout)

    def test_until_relative_format(self):
        """Test --until with relative time format."""
        self._create_tracebuffer("UntilRelative")

        # Filter until now + 1 hour - should include all traces
        result = clltk("decode", self.tmp_dir.name, "--until", "+1h")
        self.assertEqual(result.returncode, 0, msg=result.stderr)
        self.assertIn("UntilRelative", result.stdout)

    def test_since_now_anchor(self):
        """Test --since with now anchor."""
        self._create_tracebuffer("SinceNow")

        # Filter from now-1h should include recent traces
        result = clltk("decode", self.tmp_dir.name, "--since", "now-1h")
        self.assertEqual(result.returncode, 0, msg=result.stderr)
        self.assertIn("SinceNow", result.stdout)

    def test_until_now_anchor(self):
        """Test --until with now anchor."""
        self._create_tracebuffer("UntilNow")

        # Filter until now+1h should include all traces
        result = clltk("decode", self.tmp_dir.name, "--until", "now+1h")
        self.assertEqual(result.returncode, 0, msg=result.stderr)
        self.assertIn("UntilNow", result.stdout)

    def test_since_min_anchor(self):
        """Test --since with min anchor (trace start)."""
        self._create_tracebuffer_with_messages("SinceMin", ["first", "second", "third"])

        # Filter from min+0s should include all traces
        result = clltk("decode", self.tmp_dir.name, "--since", "min")
        self.assertEqual(result.returncode, 0, msg=result.stderr)
        self.assertIn("first", result.stdout)
        self.assertIn("third", result.stdout)

    def test_until_max_anchor(self):
        """Test --until with max anchor (trace end)."""
        self._create_tracebuffer_with_messages("UntilMax", ["first", "second", "third"])

        # Filter until max should include all traces
        result = clltk("decode", self.tmp_dir.name, "--until", "max")
        self.assertEqual(result.returncode, 0, msg=result.stderr)
        self.assertIn("first", result.stdout)
        self.assertIn("third", result.stdout)

    def test_since_until_combined(self):
        """Test --since and --until together."""
        self._create_tracebuffer("TimeRange")

        # Filter from 1 hour ago to 1 hour from now
        result = clltk("decode", self.tmp_dir.name, "--since", "-1h", "--until", "+1h")
        self.assertEqual(result.returncode, 0, msg=result.stderr)
        self.assertIn("TimeRange", result.stdout)

    def test_since_future_excludes_all(self):
        """Test --since in future excludes all traces."""
        self._create_tracebuffer("FutureSince")

        # Filter from 1 hour in the future - should exclude all
        result = clltk("decode", self.tmp_dir.name, "--since", "+1h")
        self.assertEqual(result.returncode, 0, msg=result.stderr)
        self.assertNotIn("test message 42", result.stdout)

    def test_until_past_excludes_all(self):
        """Test --until in past excludes all traces."""
        self._create_tracebuffer("PastUntil")

        # Filter until 1 hour ago - should exclude recent traces
        result = clltk("decode", self.tmp_dir.name, "--until", "-1h")
        self.assertEqual(result.returncode, 0, msg=result.stderr)
        self.assertNotIn("test message 42", result.stdout)

    def test_time_filter_with_duration_suffixes(self):
        """Test time filters with various duration suffixes."""
        self._create_tracebuffer("DurationSuffix")

        # Test various suffixes
        for suffix in ["-60s", "-1m", "-1h"]:
            result = clltk("decode", self.tmp_dir.name, "--since", suffix)
            self.assertEqual(
                result.returncode,
                0,
                msg=f"Failed with suffix {suffix}: {result.stderr}",
            )


class TestDecodeEdgeCases(DecodeTestCase):
    """
    Edge case tests for decode command.

    Tests error handling and boundary conditions.
    """

    def test_decode_empty_directory(self):
        """Test decode on empty directory shows no results."""
        result = clltk("decode", self.tmp_dir.name, check=False)
        self.assertEqual(result.returncode, 0)
        # Should not crash, output may be empty or just header

    def test_decode_nonexistent_path(self):
        """Test decode handles nonexistent path gracefully."""
        result = clltk("decode", "/nonexistent/path/to/file", check=False)
        # Should handle gracefully, may return 0 with no output or error
        # The important thing is it doesn't crash

    def test_decode_invalid_regex_filter(self):
        """Test decode handles invalid regex in --filter gracefully."""
        self._create_tracebuffer("RegexTest")

        result = clltk("decode", self.tmp_dir.name, "--filter", "[invalid", check=False)
        # Should fail gracefully with invalid regex
        self.assertNotEqual(result.returncode, 0)

    def test_decode_invalid_msg_regex(self):
        """Test decode handles invalid --msg-regex gracefully."""
        self._create_tracebuffer("MsgRegexInvalid")

        result = clltk(
            "decode", self.tmp_dir.name, "--msg-regex", "[invalid", check=False
        )
        # Should fail gracefully with invalid regex
        self.assertNotEqual(result.returncode, 0)

    def test_decode_invalid_time_format(self):
        """Test decode handles invalid time format gracefully."""
        self._create_tracebuffer("TimeInvalid")

        result = clltk(
            "decode", self.tmp_dir.name, "--since", "not-a-time", check=False
        )
        # Should fail with invalid time format
        self.assertNotEqual(result.returncode, 0)

    def test_decode_invalid_source_type(self):
        """Test decode handles invalid --source value gracefully."""
        self._create_tracebuffer("SourceInvalid")

        result = clltk("decode", self.tmp_dir.name, "--source", "invalid", check=False)
        # Should fail with invalid source type
        self.assertNotEqual(result.returncode, 0)

    def test_decode_multiple_tracebuffers(self):
        """Test decode handles multiple tracebuffers correctly."""
        for i in range(5):
            self._create_tracebuffer(f"Multi{i}")

        result = clltk("decode", self.tmp_dir.name)
        self.assertEqual(result.returncode, 0, msg=result.stderr)

        for i in range(5):
            self.assertIn(f"Multi{i}", result.stdout)

    def test_decode_invalid_file(self):
        """Test decode handles invalid/corrupted trace file gracefully."""
        bad_file = pathlib.Path(self.tmp_dir.name) / "invalid.clltk_trace"
        bad_file.write_bytes(b"not a valid trace file content")

        result = clltk("decode", str(bad_file), check=False)
        # Should not crash, may return error or empty result

    def test_decode_empty_tracebuffer(self):
        """Test decode on tracebuffer with no tracepoints."""
        result = clltk("buffer", "--buffer", "EmptyBuffer", "--size", "1KB")
        self.assertEqual(result.returncode, 0)

        # Don't write any tracepoints
        trace_file = pathlib.Path(self.tmp_dir.name) / "EmptyBuffer.clltk_trace"
        result = clltk("decode", str(trace_file))
        self.assertEqual(result.returncode, 0)
        # Should have header but no data lines

    def test_decode_output_file_creation(self):
        """Test --output creates file in non-existent directory fails gracefully."""
        self._create_tracebuffer("OutputDir")

        result = clltk(
            "decode",
            self.tmp_dir.name,
            "--output",
            "/nonexistent/dir/output.txt",
            check=False,
        )
        # Should fail to create file in non-existent directory
        self.assertNotEqual(result.returncode, 0)

    def test_decode_while_tracing_active(self):
        """Test decode can read while tracepoints are being written."""
        self._create_tracebuffer("ConcurrentDecode")

        # Write additional tracepoints
        for i in range(10):
            result = clltk("trace", "ConcurrentDecode", f"concurrent msg {i}")
            self.assertEqual(result.returncode, 0)

        # Decode should still work correctly
        result = clltk("decode", self.tmp_dir.name)
        self.assertEqual(result.returncode, 0)
        self.assertIn("ConcurrentDecode", result.stdout)

    def test_decode_alias(self):
        """Test that 'de' alias works for decode command."""
        self._create_tracebuffer("AliasTest")

        result = clltk("de", self.tmp_dir.name)
        self.assertEqual(result.returncode, 0, msg=result.stderr)
        self.assertIn("AliasTest", result.stdout)


class TestDecodeCompression(DecodeTestCase):
    """
    Decode command compression tests.

    Tests gzip compression option for output.
    """

    def test_compress_help_shows_option(self):
        """Test that --compress option is shown in help."""
        result = clltk("decode", "--help")
        self.assertEqual(result.returncode, 0)
        self.assertIn("--compress", result.stdout)
        self.assertIn("-z", result.stdout)

    def test_compress_output_to_file(self):
        """Test --compress writes valid gzip file."""
        self._create_tracebuffer("CompressFile")

        output_file = pathlib.Path(self.tmp_dir.name) / "output.gz"
        result = clltk("decode", self.tmp_dir.name, "-z", "-o", str(output_file))

        self.assertEqual(result.returncode, 0, msg=result.stderr)
        self.assertTrue(output_file.exists())

        # Verify it's a valid gzip file by decompressing
        with gzip.open(output_file, "rt") as f:
            content = f.read()

        self.assertIn("CompressFile", content)
        self.assertIn("test message 42", content)

    def test_compress_smaller_than_uncompressed(self):
        """Test that compressed output is smaller than uncompressed."""
        # Create multiple tracepoints to have meaningful compression
        self._create_tracebuffer_with_messages(
            "CompressSize",
            ["message " + str(i) * 50 for i in range(20)],
            size="8KB",
        )

        uncompressed_file = pathlib.Path(self.tmp_dir.name) / "uncompressed.txt"
        compressed_file = pathlib.Path(self.tmp_dir.name) / "compressed.gz"

        # Write uncompressed
        result = clltk("decode", self.tmp_dir.name, "-o", str(uncompressed_file))
        self.assertEqual(result.returncode, 0, msg=result.stderr)

        # Write compressed
        result = clltk("decode", self.tmp_dir.name, "-z", "-o", str(compressed_file))
        self.assertEqual(result.returncode, 0, msg=result.stderr)

        # Compare sizes
        uncompressed_size = uncompressed_file.stat().st_size
        compressed_size = compressed_file.stat().st_size

        self.assertGreater(uncompressed_size, 0)
        self.assertGreater(compressed_size, 0)
        self.assertLess(
            compressed_size,
            uncompressed_size,
            f"Compressed ({compressed_size}) should be smaller than uncompressed ({uncompressed_size})",
        )

    def test_compress_with_json(self):
        """Test --compress works with --json output."""
        self._create_tracebuffer("CompressJson")

        output_file = pathlib.Path(self.tmp_dir.name) / "output.json.gz"
        result = clltk("decode", self.tmp_dir.name, "-z", "-j", "-o", str(output_file))

        self.assertEqual(result.returncode, 0, msg=result.stderr)
        self.assertTrue(output_file.exists())

        # Decompress and verify JSON
        with gzip.open(output_file, "rt") as f:
            content = f.read()

        lines = [line for line in content.strip().split("\n") if line]
        self.assertGreater(len(lines), 0)

        # Each line should be valid JSON
        for line in lines:
            data = json.loads(line)
            self.assertIsInstance(data, dict)

    def test_compress_stdout(self):
        """Test --compress to stdout produces gzip data."""
        import subprocess

        self._create_tracebuffer("CompressStdout")

        # Run directly with subprocess to get raw binary output
        from helpers.base import get_build_dir

        clltk_path = get_build_dir() / "command_line_tool" / "clltk"
        result = subprocess.run(
            [str(clltk_path), "decode", self.tmp_dir.name, "-z"],
            capture_output=True,
        )

        self.assertEqual(result.returncode, 0, msg=result.stderr.decode())

        # stdout should contain gzip data (binary)
        # Verify gzip magic number (1f 8b)
        self.assertGreater(len(result.stdout), 2)
        self.assertEqual(result.stdout[0], 0x1F, "First byte should be gzip magic 0x1f")
        self.assertEqual(
            result.stdout[1], 0x8B, "Second byte should be gzip magic 0x8b"
        )

        # Decompress and verify content
        decompressed = gzip.decompress(result.stdout).decode("utf-8")
        self.assertIn("CompressStdout", decompressed)
        self.assertIn("test message 42", decompressed)

    def test_compress_with_filter(self):
        """Test --compress works with --filter option."""
        self._create_tracebuffer("CompressFilterA")
        self._create_tracebuffer("CompressFilterB")

        output_file = pathlib.Path(self.tmp_dir.name) / "filtered.gz"
        result = clltk(
            "decode",
            self.tmp_dir.name,
            "-z",
            "-o",
            str(output_file),
            "--filter",
            "CompressFilterA",
        )

        self.assertEqual(result.returncode, 0, msg=result.stderr)

        with gzip.open(output_file, "rt") as f:
            content = f.read()

        self.assertIn("CompressFilterA", content)
        self.assertNotIn("CompressFilterB", content)

    def test_compress_empty_directory(self):
        """Test --compress on empty directory produces valid empty gzip."""
        # Use a fresh empty directory
        empty_dir = pathlib.Path(self.tmp_dir.name) / "empty_subdir"
        empty_dir.mkdir()

        output_file = pathlib.Path(self.tmp_dir.name) / "empty.gz"
        result = clltk("decode", str(empty_dir), "-z", "-o", str(output_file))

        self.assertEqual(result.returncode, 0, msg=result.stderr)
        self.assertTrue(output_file.exists())

        # Should be valid gzip (may just have header)
        with gzip.open(output_file, "rt") as f:
            content = f.read()
        # Content may be empty or just header, that's fine

    def test_compress_large_output(self):
        """Test --compress handles large output correctly."""
        # Create many tracepoints
        messages = [f"large_message_{i}_" + "x" * 100 for i in range(50)]
        self._create_tracebuffer_with_messages("CompressLarge", messages, size="64KB")

        output_file = pathlib.Path(self.tmp_dir.name) / "large.gz"
        result = clltk("decode", self.tmp_dir.name, "-z", "-o", str(output_file))

        self.assertEqual(result.returncode, 0, msg=result.stderr)

        # Decompress and verify all messages are present
        with gzip.open(output_file, "rt") as f:
            content = f.read()

        for i in range(50):
            self.assertIn(f"large_message_{i}_", content)


if __name__ == "__main__":
    unittest.main()
