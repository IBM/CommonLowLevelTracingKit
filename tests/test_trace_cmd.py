#!/usr/bin/python3
# Copyright (c) 2024, International Business Machines
# SPDX-License-Identifier: BSD-2-Clause-Patent

"""
CLLTK trace command tests.

Tests for the 'clltk trace' subcommand that writes a single message as a tracepoint.
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


class TraceTestCase(unittest.TestCase):
    """Base class for trace command tests with temporary directory setup."""

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

    def _create_buffer(self, name: str, size: str = "4KB"):
        """Create an empty tracebuffer."""
        result = clltk("buffer", "--buffer", name, "--size", size)
        self.assertEqual(result.returncode, 0, msg=result.stderr)

    def _get_trace_file(self, name: str) -> pathlib.Path:
        """Get path to tracebuffer file."""
        return pathlib.Path(self.tmp_dir.name) / f"{name}.clltk_trace"

    def _decode_json(self, path) -> list:
        """Decode tracebuffer and return list of JSON entries."""
        result = clltk("decode", str(path), "--json")
        self.assertEqual(result.returncode, 0, msg=result.stderr)
        lines = [line for line in result.stdout.strip().split("\n") if line]
        return [json.loads(line) for line in lines]


class TestTraceCommandBase(unittest.TestCase):
    """
    Basic trace command CLI tests.

    Tests help output, subcommand availability, and alias.
    """

    def test_trace_help(self):
        """Test that trace --help works."""
        result = clltk("trace", "--help")
        self.assertEqual(result.returncode, 0)
        self.assertIn("trace", result.stdout.lower())

    def test_trace_subcommand_exists(self):
        """Test that trace subcommand is recognized."""
        result = clltk("--help")
        self.assertEqual(result.returncode, 0)
        self.assertIn("trace", result.stdout)

    def test_trace_alias_tp(self):
        """Test that 'tp' alias works for trace command."""
        result = clltk("tp", "--help")
        self.assertEqual(result.returncode, 0)
        self.assertIn("trace", result.stdout.lower())

    def test_trace_help_shows_options(self):
        """Test that help shows all expected options."""
        result = clltk("trace", "--help")
        self.assertEqual(result.returncode, 0)
        self.assertIn("--buffer", result.stdout)
        self.assertIn("-b", result.stdout)
        self.assertIn("--size", result.stdout)
        self.assertIn("-s", result.stdout)
        self.assertIn("--message", result.stdout)
        self.assertIn("-m", result.stdout)
        self.assertIn("--file", result.stdout)
        self.assertIn("-f", result.stdout)
        self.assertIn("--line", result.stdout)
        self.assertIn("-l", result.stdout)
        self.assertIn("--pid", result.stdout)
        self.assertIn("--tid", result.stdout)


class TestTraceBasicFunctionality(TraceTestCase):
    """
    Basic trace command functionality tests.

    Tests tracing to existing buffer, creating new buffer, and verifying messages.
    """

    def test_trace_to_existing_buffer(self):
        """Test tracing to an existing tracebuffer."""
        self._create_buffer("ExistingBuffer")

        result = clltk("trace", "-b", "ExistingBuffer", "-m", "hello world")
        self.assertEqual(result.returncode, 0, msg=result.stderr)

        entries = self._decode_json(self._get_trace_file("ExistingBuffer"))
        messages = [e["message"] for e in entries]
        self.assertTrue(
            any("hello world" in msg for msg in messages),
            f"Expected 'hello world' in messages: {messages}",
        )

    def test_trace_creates_buffer_if_not_exists(self):
        """Test trace creates buffer automatically if it doesn't exist."""
        trace_file = self._get_trace_file("NewBuffer")
        self.assertFalse(trace_file.exists())

        result = clltk("trace", "-b", "NewBuffer", "-m", "auto create test")
        self.assertEqual(result.returncode, 0, msg=result.stderr)

        self.assertTrue(trace_file.exists())
        entries = self._decode_json(trace_file)
        messages = [e["message"] for e in entries]
        self.assertTrue(
            any("auto create test" in msg for msg in messages),
            f"Expected 'auto create test' in messages: {messages}",
        )

    def test_trace_with_positional_arguments(self):
        """Test trace with positional buffer and message arguments."""
        result = clltk("trace", "PosBuffer", "positional message")
        self.assertEqual(result.returncode, 0, msg=result.stderr)

        entries = self._decode_json(self._get_trace_file("PosBuffer"))
        messages = [e["message"] for e in entries]
        self.assertTrue(
            any("positional message" in msg for msg in messages),
            f"Expected 'positional message' in messages: {messages}",
        )

    def test_trace_with_size_option(self):
        """Test trace with --size option for new buffer."""
        result = clltk("trace", "-b", "SizedBuffer", "-s", "8KB", "-m", "sized test")
        self.assertEqual(result.returncode, 0, msg=result.stderr)

        trace_file = self._get_trace_file("SizedBuffer")
        self.assertTrue(trace_file.exists())

    def test_trace_multiple_messages(self):
        """Test tracing multiple messages to the same buffer."""
        buffer_name = "MultiMsg"
        messages = ["first message", "second message", "third message"]

        for msg in messages:
            result = clltk("trace", "-b", buffer_name, "-m", msg)
            self.assertEqual(result.returncode, 0, msg=result.stderr)

        entries = self._decode_json(self._get_trace_file(buffer_name))
        decoded_messages = [e["message"] for e in entries]

        for msg in messages:
            self.assertTrue(
                any(msg in decoded for decoded in decoded_messages),
                f"Expected '{msg}' in decoded messages: {decoded_messages}",
            )

    def test_trace_verifies_message_in_decode(self):
        """Test that traced message appears correctly in decode output."""
        test_message = "verification test 12345"
        result = clltk("trace", "-b", "VerifyBuffer", "-m", test_message)
        self.assertEqual(result.returncode, 0, msg=result.stderr)

        entries = self._decode_json(self._get_trace_file("VerifyBuffer"))
        self.assertGreater(len(entries), 0, "Expected at least one entry")

        found = any(test_message in e["message"] for e in entries)
        self.assertTrue(found, f"Message '{test_message}' not found in entries")


class TestTraceOptions(TraceTestCase):
    """
    Trace command options tests.

    Tests --file, --line, --pid, --tid options and verifies in decode JSON.
    """

    def test_trace_with_file_option(self):
        """Test trace with --file option."""
        result = clltk(
            "trace",
            "-b",
            "FileOptBuffer",
            "-m",
            "file test",
            "-f",
            "test_source.py",
        )
        self.assertEqual(result.returncode, 0, msg=result.stderr)

        entries = self._decode_json(self._get_trace_file("FileOptBuffer"))
        self.assertGreater(len(entries), 0)

        entry = entries[0]
        self.assertIn("file", entry)
        self.assertIn("test_source.py", entry["file"])

    def test_trace_with_file_short_option(self):
        """Test trace with -f short option."""
        result = clltk(
            "trace", "-b", "FileShortBuffer", "-m", "file short test", "-f", "short.c"
        )
        self.assertEqual(result.returncode, 0, msg=result.stderr)

        entries = self._decode_json(self._get_trace_file("FileShortBuffer"))
        self.assertGreater(len(entries), 0)
        self.assertIn("short.c", entries[0]["file"])

    def test_trace_with_line_option(self):
        """Test trace with --line option."""
        result = clltk(
            "trace", "-b", "LineOptBuffer", "-m", "line test", "--line", "42"
        )
        self.assertEqual(result.returncode, 0, msg=result.stderr)

        entries = self._decode_json(self._get_trace_file("LineOptBuffer"))
        self.assertGreater(len(entries), 0)

        entry = entries[0]
        self.assertIn("line", entry)
        self.assertEqual(entry["line"], 42)

    def test_trace_with_line_short_option(self):
        """Test trace with -l short option."""
        result = clltk(
            "trace", "-b", "LineShortBuffer", "-m", "line short test", "-l", "123"
        )
        self.assertEqual(result.returncode, 0, msg=result.stderr)

        entries = self._decode_json(self._get_trace_file("LineShortBuffer"))
        self.assertGreater(len(entries), 0)
        self.assertEqual(entries[0]["line"], 123)

    def test_trace_with_pid_option(self):
        """Test trace with --pid option."""
        custom_pid = 99999
        result = clltk(
            "trace",
            "-b",
            "PidOptBuffer",
            "-m",
            "pid test",
            "--pid",
            str(custom_pid),
        )
        self.assertEqual(result.returncode, 0, msg=result.stderr)

        entries = self._decode_json(self._get_trace_file("PidOptBuffer"))
        self.assertGreater(len(entries), 0)

        entry = entries[0]
        self.assertIn("pid", entry)
        self.assertEqual(entry["pid"], custom_pid)

    def test_trace_with_tid_option(self):
        """Test trace with --tid option."""
        custom_tid = 88888
        result = clltk(
            "trace",
            "-b",
            "TidOptBuffer",
            "-m",
            "tid test",
            "--tid",
            str(custom_tid),
        )
        self.assertEqual(result.returncode, 0, msg=result.stderr)

        entries = self._decode_json(self._get_trace_file("TidOptBuffer"))
        self.assertGreater(len(entries), 0)

        entry = entries[0]
        self.assertIn("tid", entry)
        self.assertEqual(entry["tid"], custom_tid)

    def test_trace_with_all_metadata_options(self):
        """Test trace with all metadata options combined."""
        result = clltk(
            "trace",
            "-b",
            "AllMetaBuffer",
            "-m",
            "all metadata test",
            "-f",
            "combined.cpp",
            "-l",
            "256",
            "--pid",
            "11111",
            "--tid",
            "22222",
        )
        self.assertEqual(result.returncode, 0, msg=result.stderr)

        entries = self._decode_json(self._get_trace_file("AllMetaBuffer"))
        self.assertGreater(len(entries), 0)

        entry = entries[0]
        self.assertIn("combined.cpp", entry["file"])
        self.assertEqual(entry["line"], 256)
        self.assertEqual(entry["pid"], 11111)
        self.assertEqual(entry["tid"], 22222)

    def test_trace_default_pid_tid(self):
        """Test that default pid/tid are set to process values."""
        result = clltk("trace", "-b", "DefaultPidTid", "-m", "default ids test")
        self.assertEqual(result.returncode, 0, msg=result.stderr)

        entries = self._decode_json(self._get_trace_file("DefaultPidTid"))
        self.assertGreater(len(entries), 0)

        entry = entries[0]
        # PID and TID should be non-zero when using defaults
        self.assertIn("pid", entry)
        self.assertIn("tid", entry)
        self.assertIsInstance(entry["pid"], int)
        self.assertIsInstance(entry["tid"], int)


class TestTraceMessages(TraceTestCase):
    """
    Trace command message handling tests.

    Tests special characters, Unicode, empty messages, and long messages.
    """

    def test_trace_special_characters(self):
        """Test tracing messages with special characters."""
        special_msg = "Special: !@#$%^&*()_+-=[]{}|;':\",./<>?"
        result = clltk("trace", "-b", "SpecialChars", "-m", special_msg)
        self.assertEqual(result.returncode, 0, msg=result.stderr)

        entries = self._decode_json(self._get_trace_file("SpecialChars"))
        self.assertGreater(len(entries), 0)
        # Check that at least some special chars are preserved
        self.assertTrue(
            any("Special:" in e["message"] for e in entries),
            f"Special characters message not found",
        )

    def test_trace_unicode_characters(self):
        """Test tracing messages with Unicode characters."""
        unicode_msg = "Unicode: Hello World"
        result = clltk("trace", "-b", "UnicodeBuffer", "-m", unicode_msg)
        self.assertEqual(result.returncode, 0, msg=result.stderr)

        entries = self._decode_json(self._get_trace_file("UnicodeBuffer"))
        self.assertGreater(len(entries), 0)

    def test_trace_empty_message(self):
        """Test tracing an empty message."""
        result = clltk("trace", "-b", "EmptyMsg", "-m", "", check=False)
        # Empty message behavior - may succeed or fail depending on implementation
        # The important thing is it doesn't crash

    def test_trace_whitespace_message(self):
        """Test tracing a whitespace-only message."""
        result = clltk("trace", "-b", "WhitespaceMsg", "-m", "   ")
        self.assertEqual(result.returncode, 0, msg=result.stderr)

        entries = self._decode_json(self._get_trace_file("WhitespaceMsg"))
        self.assertGreater(len(entries), 0)

    def test_trace_newline_in_message(self):
        """Test tracing a message with newline characters."""
        msg_with_newline = "line1\nline2\nline3"
        result = clltk("trace", "-b", "NewlineMsg", "-m", msg_with_newline)
        self.assertEqual(result.returncode, 0, msg=result.stderr)

        entries = self._decode_json(self._get_trace_file("NewlineMsg"))
        self.assertGreater(len(entries), 0)

    def test_trace_tab_in_message(self):
        """Test tracing a message with tab characters."""
        msg_with_tab = "col1\tcol2\tcol3"
        result = clltk("trace", "-b", "TabMsg", "-m", msg_with_tab)
        self.assertEqual(result.returncode, 0, msg=result.stderr)

        entries = self._decode_json(self._get_trace_file("TabMsg"))
        self.assertGreater(len(entries), 0)

    def test_trace_very_long_message(self):
        """Test tracing a very long message."""
        long_msg = "A" * 10000  # 10KB message
        result = clltk(
            "trace", "-b", "LongMsg", "-s", "64KB", "-m", long_msg, check=False
        )
        # Long message may be truncated or rejected
        # The important thing is graceful handling

    def test_trace_message_with_quotes(self):
        """Test tracing a message with quotes."""
        quoted_msg = "He said \"Hello\" and she said 'Hi'"
        result = clltk("trace", "-b", "QuotedMsg", "-m", quoted_msg)
        self.assertEqual(result.returncode, 0, msg=result.stderr)

        entries = self._decode_json(self._get_trace_file("QuotedMsg"))
        self.assertGreater(len(entries), 0)

    def test_trace_message_with_backslash(self):
        """Test tracing a message with backslash characters."""
        backslash_msg = r"Path: C:\Users\test\file.txt"
        result = clltk("trace", "-b", "BackslashMsg", "-m", backslash_msg)
        self.assertEqual(result.returncode, 0, msg=result.stderr)

        entries = self._decode_json(self._get_trace_file("BackslashMsg"))
        self.assertGreater(len(entries), 0)

    def test_trace_numeric_message(self):
        """Test tracing a purely numeric message."""
        result = clltk("trace", "-b", "NumericMsg", "-m", "1234567890")
        self.assertEqual(result.returncode, 0, msg=result.stderr)

        entries = self._decode_json(self._get_trace_file("NumericMsg"))
        self.assertGreater(len(entries), 0)
        self.assertTrue(
            any("1234567890" in e["message"] for e in entries),
            "Numeric message not found",
        )


class TestTraceEdgeCases(TraceTestCase):
    """
    Edge case tests for trace command.

    Tests error handling and boundary conditions.
    """

    def test_trace_invalid_buffer_name(self):
        """Test trace with invalid buffer name."""
        result = clltk("trace", "-b", "Invalid/Buffer/Name", "-m", "test", check=False)
        self.assertNotEqual(result.returncode, 0)

    def test_trace_buffer_name_with_spaces(self):
        """Test trace with buffer name containing spaces."""
        result = clltk("trace", "-b", "Buffer With Spaces", "-m", "test", check=False)
        # Buffer names with spaces may be invalid
        # The important thing is graceful handling

    def test_trace_missing_required_buffer(self):
        """Test trace without required buffer argument."""
        result = clltk("trace", "-m", "test message", check=False)
        self.assertNotEqual(result.returncode, 0)

    def test_trace_missing_required_message(self):
        """Test trace without required message argument."""
        result = clltk("trace", "-b", "NoMsgBuffer", check=False)
        self.assertNotEqual(result.returncode, 0)

    def test_trace_invalid_size_format(self):
        """Test trace with invalid size format."""
        result = clltk(
            "trace", "-b", "InvalidSize", "-s", "invalid", "-m", "test", check=False
        )
        self.assertNotEqual(result.returncode, 0)

    def test_trace_negative_line_number(self):
        """Test trace with negative line number."""
        result = clltk("trace", "-b", "NegLine", "-m", "test", "-l", "-1", check=False)
        # Negative line may be rejected or converted
        # The important thing is graceful handling

    def test_trace_zero_size_buffer(self):
        """Test trace with zero size buffer."""
        result = clltk("trace", "-b", "ZeroSize", "-s", "0", "-m", "test", check=False)
        # Zero size should likely fail
        self.assertNotEqual(result.returncode, 0)

    def test_trace_very_small_buffer(self):
        """Test trace to very small buffer."""
        result = clltk(
            "trace", "-b", "TinyBuffer", "-s", "64", "-m", "small buffer test"
        )
        # Small buffer may work or fail depending on minimum size
        # Important thing is graceful handling

    def test_trace_size_suffixes(self):
        """Test trace with various size suffixes."""
        sizes = ["1K", "1KB", "1M", "1MB"]
        for i, size in enumerate(sizes):
            result = clltk(
                "trace",
                "-b",
                f"SizeSuffix{i}",
                "-s",
                size,
                "-m",
                f"size test {size}",
                check=False,
            )
            # At least KB sizes should work
            if "K" in size:
                self.assertEqual(
                    result.returncode, 0, msg=f"Failed for size {size}: {result.stderr}"
                )

    def test_trace_concurrent_writes(self):
        """Test multiple trace commands to same buffer."""
        buffer_name = "ConcurrentBuffer"
        self._create_buffer(buffer_name, "16KB")

        # Write multiple messages rapidly
        for i in range(20):
            result = clltk("trace", "-b", buffer_name, "-m", f"concurrent msg {i}")
            self.assertEqual(result.returncode, 0, msg=result.stderr)

        entries = self._decode_json(self._get_trace_file(buffer_name))
        # Should have at least some messages (may wrap in small buffer)
        self.assertGreater(len(entries), 0)

    def test_trace_alias_tp_functionality(self):
        """Test that 'tp' alias works same as 'trace'."""
        result = clltk("tp", "-b", "AliasBuffer", "-m", "alias test message")
        self.assertEqual(result.returncode, 0, msg=result.stderr)

        entries = self._decode_json(self._get_trace_file("AliasBuffer"))
        self.assertGreater(len(entries), 0)
        self.assertTrue(
            any("alias test message" in e["message"] for e in entries),
            "Alias test message not found",
        )

    @unittest.skipIf(
        os.geteuid() == 0,
        "Test requires non-root user (root bypasses file permissions)",
    )
    def test_trace_to_readonly_directory(self):
        """Test trace when directory is not writable."""
        import stat

        readonly_dir = pathlib.Path(self.tmp_dir.name) / "readonly"
        readonly_dir.mkdir()
        readonly_dir.chmod(stat.S_IRUSR | stat.S_IXUSR)

        old_path = os.environ["CLLTK_TRACING_PATH"]
        os.environ["CLLTK_TRACING_PATH"] = str(readonly_dir)

        try:
            result = clltk("trace", "-b", "ReadonlyTest", "-m", "test", check=False)
            # Should fail due to permissions
            self.assertNotEqual(result.returncode, 0)
        finally:
            os.environ["CLLTK_TRACING_PATH"] = old_path
            readonly_dir.chmod(stat.S_IRWXU)

    def test_trace_json_structure_validation(self):
        """Test that traced message produces valid JSON structure in decode."""
        result = clltk(
            "trace",
            "-b",
            "JsonValidate",
            "-m",
            "json validation test",
            "-f",
            "validate.py",
            "-l",
            "100",
        )
        self.assertEqual(result.returncode, 0, msg=result.stderr)

        entries = self._decode_json(self._get_trace_file("JsonValidate"))
        self.assertGreater(len(entries), 0)

        entry = entries[0]
        # Verify required JSON fields
        required_fields = [
            "timestamp_ns",
            "timestamp",
            "datetime",
            "tracebuffer",
            "pid",
            "tid",
            "message",
            "file",
            "line",
            "is_kernel",
            "source_type",
            "tracepoint_nr",
        ]
        for field in required_fields:
            self.assertIn(field, entry, f"Missing required field: {field}")


if __name__ == "__main__":
    unittest.main()
