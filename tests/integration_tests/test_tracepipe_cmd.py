#!/usr/bin/python3
# Copyright (c) 2024, International Business Machines
# SPDX-License-Identifier: BSD-2-Clause-Patent

"""
CLLTK tracepipe command tests.

Tests for the 'clltk tracepipe' subcommand that reads lines from stdin or
a file and writes each as a tracepoint to a tracebuffer.
"""

import json
import os
import pathlib
import subprocess
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.realpath(__file__))))

from helpers.base import run_command, clltk_cmd_file
from helpers.clltk_cmd import clltk


def setUpModule():
    """Build clltk-cmd before running tests."""
    run_command("cmake --preset default")
    run_command("cmake --build --preset default --target clltk-cmd")


class TracepipeTestCase(unittest.TestCase):
    """Base class for tracepipe tests with temporary directory setup."""

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

    def _list_trace_files(self, path=None) -> list:
        """List .clltk_trace files in the given path."""
        if path is None:
            path = self.tmp_dir.name
        trace_path = pathlib.Path(path)
        return list(trace_path.glob("*.clltk_trace"))

    def _decode_buffer(self, buffer_name: str) -> str:
        """Decode a tracebuffer and return the output."""
        trace_file = pathlib.Path(self.tmp_dir.name) / f"{buffer_name}.clltk_trace"
        result = clltk("decode", str(trace_file))
        return result.stdout

    def _create_input_file(self, lines: list) -> str:
        """Create a temporary input file with the given lines."""
        input_file = pathlib.Path(self.tmp_dir.name) / "input.txt"
        input_file.write_text("\n".join(lines) + "\n")
        return str(input_file)


class TestTracepipeCommandBase(unittest.TestCase):
    """
    Basic tracepipe command CLI tests.

    Tests help output and subcommand availability.
    """

    def test_tracepipe_help(self):
        """Test that tracepipe --help works."""
        result = clltk("tracepipe", "--help")
        self.assertEqual(result.returncode, 0)
        self.assertIn("tracepipe", result.stdout.lower())
        self.assertIn("--buffer", result.stdout)
        self.assertIn("--json", result.stdout)

    def test_tracepipe_subcommand_exists(self):
        """Test that tracepipe subcommand is recognized."""
        result = clltk("--help")
        self.assertEqual(result.returncode, 0)
        self.assertIn("tracepipe", result.stdout)


class TestTracepipeBasicFunctionality(TracepipeTestCase):
    """
    Basic tracepipe functionality tests.

    Tests reading from files and writing tracepoints.
    """

    def test_pipe_from_file(self):
        """Test tracepipe reads from a file and creates tracepoints."""
        buffer_name = "FileInputBuffer"
        messages = ["message_one", "message_two", "message_three"]
        input_file = self._create_input_file(messages)

        result = clltk("tracepipe", "--buffer", buffer_name, input_file)
        self.assertEqual(result.returncode, 0, msg=result.stderr)

        # Verify tracebuffer was created
        files = self._list_trace_files()
        self.assertEqual(len(files), 1)
        self.assertEqual(files[0].name, f"{buffer_name}.clltk_trace")

        # Decode and verify messages
        decoded = self._decode_buffer(buffer_name)
        for msg in messages:
            self.assertIn(msg, decoded, f"Message '{msg}' not found in decoded output")

    def test_multiple_lines_create_multiple_tracepoints(self):
        """Test that each line creates a separate tracepoint."""
        buffer_name = "MultiLineBuffer"
        num_lines = 10
        messages = [f"line_{i:03d}" for i in range(num_lines)]
        input_file = self._create_input_file(messages)

        result = clltk("tracepipe", "-b", buffer_name, input_file)
        self.assertEqual(result.returncode, 0, msg=result.stderr)

        # Decode and verify all messages are present
        decoded = self._decode_buffer(buffer_name)
        for msg in messages:
            self.assertIn(msg, decoded, f"Message '{msg}' not found")

    def test_tracepipe_with_size_option(self):
        """Test tracepipe with custom buffer size."""
        buffer_name = "SizedBuffer"
        messages = ["sized_message"]
        input_file = self._create_input_file(messages)

        result = clltk("tracepipe", "-b", buffer_name, "-s", "4KB", input_file)
        self.assertEqual(result.returncode, 0, msg=result.stderr)

        files = self._list_trace_files()
        self.assertEqual(len(files), 1)

        decoded = self._decode_buffer(buffer_name)
        self.assertIn("sized_message", decoded)

    def test_tracepipe_positional_buffer_name(self):
        """Test tracepipe with positional buffer name."""
        buffer_name = "PositionalBuffer"
        messages = ["positional_test"]
        input_file = self._create_input_file(messages)

        result = clltk("tracepipe", buffer_name, input_file)
        self.assertEqual(result.returncode, 0, msg=result.stderr)

        decoded = self._decode_buffer(buffer_name)
        self.assertIn("positional_test", decoded)


class TestTracepipeStdin(TracepipeTestCase):
    """
    Tests for tracepipe reading from stdin.

    Uses subprocess with stdin pipe to feed input.
    """

    def test_read_from_stdin(self):
        """Test tracepipe reads from stdin when no file specified."""
        buffer_name = "StdinBuffer"
        messages = ["stdin_msg_1", "stdin_msg_2", "stdin_msg_3"]
        input_data = "\n".join(messages) + "\n"

        clltk_path = clltk_cmd_file()
        env = os.environ.copy()

        result = subprocess.run(
            [str(clltk_path), "tracepipe", "-b", buffer_name],
            input=input_data,
            capture_output=True,
            text=True,
            env=env,
        )
        self.assertEqual(result.returncode, 0, msg=result.stderr)

        decoded = self._decode_buffer(buffer_name)
        for msg in messages:
            self.assertIn(msg, decoded, f"Message '{msg}' not found in decoded output")

    def test_read_from_stdin_with_dash(self):
        """Test tracepipe reads from stdin when '-' specified as input file."""
        buffer_name = "StdinDashBuffer"
        messages = ["dash_stdin_msg"]
        input_data = "\n".join(messages) + "\n"

        clltk_path = clltk_cmd_file()
        env = os.environ.copy()

        result = subprocess.run(
            [str(clltk_path), "tracepipe", "-b", buffer_name, "-"],
            input=input_data,
            capture_output=True,
            text=True,
            env=env,
        )
        self.assertEqual(result.returncode, 0, msg=result.stderr)

        decoded = self._decode_buffer(buffer_name)
        self.assertIn("dash_stdin_msg", decoded)

    def test_stdin_with_popen(self):
        """Test tracepipe with Popen for streaming input."""
        buffer_name = "PopenBuffer"
        messages = ["popen_msg_1", "popen_msg_2"]

        clltk_path = clltk_cmd_file()
        env = os.environ.copy()

        proc = subprocess.Popen(
            [str(clltk_path), "tracepipe", "-b", buffer_name],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            env=env,
        )

        # Write messages to stdin
        assert proc.stdin is not None
        for msg in messages:
            proc.stdin.write((msg + "\n").encode())
        proc.stdin.flush()
        proc.stdin.close()

        # Wait for process to complete (stdin already closed, so don't use communicate)
        proc.wait(timeout=10)
        stderr = proc.stderr.read() if proc.stderr else b""
        self.assertEqual(proc.returncode, 0, msg=stderr.decode())

        decoded = self._decode_buffer(buffer_name)
        for msg in messages:
            self.assertIn(msg, decoded)


class TestTracepipeJsonMode(TracepipeTestCase):
    """
    Tests for tracepipe JSON input mode.

    Tests parsing JSON objects with message and optional fields.
    """

    def test_json_message_only(self):
        """Test JSON input with only required message field."""
        buffer_name = "JsonMsgOnly"
        json_lines = [
            json.dumps({"message": "json_only_message_1"}),
            json.dumps({"message": "json_only_message_2"}),
        ]
        input_file = self._create_input_file(json_lines)

        result = clltk("tracepipe", "-b", buffer_name, "--json", input_file)
        self.assertEqual(result.returncode, 0, msg=result.stderr)

        decoded = self._decode_buffer(buffer_name)
        self.assertIn("json_only_message_1", decoded)
        self.assertIn("json_only_message_2", decoded)

    def test_json_with_all_fields(self):
        """Test JSON input with all optional fields."""
        buffer_name = "JsonAllFields"
        json_obj = {
            "message": "full_json_message",
            "pid": 12345,
            "tid": 67890,
            "file": "test_source.cpp",
            "line": 42,
        }
        json_lines = [json.dumps(json_obj)]
        input_file = self._create_input_file(json_lines)

        result = clltk("tracepipe", "-b", buffer_name, "-j", input_file)
        self.assertEqual(result.returncode, 0, msg=result.stderr)

        decoded = self._decode_buffer(buffer_name)
        self.assertIn("full_json_message", decoded)

    def test_json_with_partial_fields(self):
        """Test JSON input with some optional fields."""
        buffer_name = "JsonPartialFields"
        json_lines = [
            json.dumps({"message": "partial_1", "pid": 111}),
            json.dumps({"message": "partial_2", "file": "myfile.c", "line": 10}),
        ]
        input_file = self._create_input_file(json_lines)

        result = clltk("tracepipe", "-b", buffer_name, "--json", input_file)
        self.assertEqual(result.returncode, 0, msg=result.stderr)

        decoded = self._decode_buffer(buffer_name)
        self.assertIn("partial_1", decoded)
        self.assertIn("partial_2", decoded)

    def test_invalid_json_handling(self):
        """Test tracepipe handles invalid JSON gracefully."""
        buffer_name = "InvalidJsonBuffer"
        # Mix of valid and invalid JSON
        lines = [
            json.dumps({"message": "valid_json_msg"}),
            "this is not valid json {",
            json.dumps({"message": "another_valid_msg"}),
        ]
        input_file = self._create_input_file(lines)

        # According to help: "If JSON parsing fails, falls back to plain text mode"
        result = clltk(
            "tracepipe", "-b", buffer_name, "--json", input_file, check=False
        )
        # Should not crash - either processes some messages or falls back to plain text

        # Check that at least some messages were processed
        files = self._list_trace_files()
        self.assertGreater(len(files), 0, "Expected tracebuffer to be created")

    def test_json_missing_message_field(self):
        """Test JSON input missing required message field."""
        buffer_name = "JsonNoMessage"
        json_lines = [
            json.dumps({"pid": 123, "tid": 456}),  # Missing 'message'
        ]
        input_file = self._create_input_file(json_lines)

        result = clltk(
            "tracepipe", "-b", buffer_name, "--json", input_file, check=False
        )
        # Should handle gracefully - may skip the line or use empty message

    def test_json_via_stdin(self):
        """Test JSON mode reading from stdin."""
        buffer_name = "JsonStdinBuffer"
        json_lines = [
            json.dumps({"message": "stdin_json_1"}),
            json.dumps({"message": "stdin_json_2", "pid": 999}),
        ]
        input_data = "\n".join(json_lines) + "\n"

        clltk_path = clltk_cmd_file()
        env = os.environ.copy()

        result = subprocess.run(
            [str(clltk_path), "tracepipe", "-b", buffer_name, "--json"],
            input=input_data,
            capture_output=True,
            text=True,
            env=env,
        )
        self.assertEqual(result.returncode, 0, msg=result.stderr)

        decoded = self._decode_buffer(buffer_name)
        self.assertIn("stdin_json_1", decoded)
        self.assertIn("stdin_json_2", decoded)


class TestTracepipeEdgeCases(TracepipeTestCase):
    """
    Edge case tests for tracepipe command.

    Tests boundary conditions and unusual input.
    """

    def test_empty_lines(self):
        """Test tracepipe handles empty lines."""
        buffer_name = "EmptyLinesBuffer"
        lines = ["first_msg", "", "second_msg", "", "", "third_msg"]
        input_file = self._create_input_file(lines)

        result = clltk("tracepipe", "-b", buffer_name, input_file)
        self.assertEqual(result.returncode, 0, msg=result.stderr)

        decoded = self._decode_buffer(buffer_name)
        self.assertIn("first_msg", decoded)
        self.assertIn("second_msg", decoded)
        self.assertIn("third_msg", decoded)

    def test_very_long_lines(self):
        """Test tracepipe handles very long lines."""
        buffer_name = "LongLinesBuffer"
        # Create a long message
        long_msg = "L" * 500
        short_msg = "short_msg"
        lines = [long_msg, short_msg]
        input_file = self._create_input_file(lines)

        result = clltk("tracepipe", "-b", buffer_name, "-s", "16KB", input_file)
        self.assertEqual(result.returncode, 0, msg=result.stderr)

        decoded = self._decode_buffer(buffer_name)
        # At least the short message should be there
        self.assertIn(short_msg, decoded)

    def test_special_characters(self):
        """Test tracepipe handles special characters in messages."""
        buffer_name = "SpecialCharsBuffer"
        messages = [
            "msg_with_numbers_12345",
            "msg_with_underscore_test",
            "msg-with-dashes",
            "msg.with.dots",
            "msg:with:colons",
            "msg=with=equals",
        ]
        input_file = self._create_input_file(messages)

        result = clltk("tracepipe", "-b", buffer_name, input_file)
        self.assertEqual(result.returncode, 0, msg=result.stderr)

        decoded = self._decode_buffer(buffer_name)
        for msg in messages:
            self.assertIn(msg, decoded, f"Message '{msg}' not found")

    def test_whitespace_in_messages(self):
        """Test tracepipe handles messages with whitespace."""
        buffer_name = "WhitespaceBuffer"
        messages = [
            "message with spaces",
            "message\twith\ttabs",
            "  leading spaces",
            "trailing spaces  ",
        ]
        input_file = self._create_input_file(messages)

        result = clltk("tracepipe", "-b", buffer_name, input_file)
        self.assertEqual(result.returncode, 0, msg=result.stderr)

        decoded = self._decode_buffer(buffer_name)
        # Check that at least the base content is preserved
        self.assertIn("message with spaces", decoded)

    def test_single_line_input(self):
        """Test tracepipe with single line input."""
        buffer_name = "SingleLineBuffer"
        lines = ["single_message"]
        input_file = self._create_input_file(lines)

        result = clltk("tracepipe", "-b", buffer_name, input_file)
        self.assertEqual(result.returncode, 0, msg=result.stderr)

        decoded = self._decode_buffer(buffer_name)
        self.assertIn("single_message", decoded)

    def test_empty_file(self):
        """Test tracepipe with empty file."""
        buffer_name = "EmptyFileBuffer"
        input_file = pathlib.Path(self.tmp_dir.name) / "empty.txt"
        input_file.write_text("")

        result = clltk("tracepipe", "-b", buffer_name, str(input_file), check=False)
        # Should not crash with empty input


class TestTracepipeErrorHandling(TracepipeTestCase):
    """
    Error handling tests for tracepipe command.

    Tests error conditions and invalid input.
    """

    def test_missing_buffer_name(self):
        """Test tracepipe fails without buffer name."""
        result = clltk("tracepipe", check=False)
        self.assertNotEqual(result.returncode, 0)

    def test_invalid_buffer_name(self):
        """Test tracepipe rejects invalid buffer names."""
        invalid_names = [
            " Buffer",  # leading space
            "_Buffer",  # leading underscore
            "8uffer",  # leading digit
        ]
        input_file = self._create_input_file(["test_msg"])

        for name in invalid_names:
            with self.subTest(name=name):
                result = clltk("tracepipe", "-b", name, input_file, check=False)
                self.assertNotEqual(
                    result.returncode,
                    0,
                    msg=f"Buffer name '{name}' should be rejected",
                )

    def test_nonexistent_input_file(self):
        """Test tracepipe handles nonexistent input file."""
        result = clltk(
            "tracepipe",
            "-b",
            "TestBuffer",
            "/nonexistent/path/to/file.txt",
            check=False,
        )
        self.assertNotEqual(result.returncode, 0)

    def test_invalid_size_format(self):
        """Test tracepipe rejects invalid size format."""
        input_file = self._create_input_file(["test"])
        result = clltk(
            "tracepipe",
            "-b",
            "TestBuffer",
            "-s",
            "invalid_size",
            input_file,
            check=False,
        )
        self.assertNotEqual(result.returncode, 0)


class TestTracepipeIntegration(TracepipeTestCase):
    """
    Integration tests for tracepipe with other commands.

    Tests tracepipe in combination with decode and other operations.
    """

    def test_tracepipe_then_decode(self):
        """Test tracepipe followed by decode shows all messages."""
        buffer_name = "IntegrationBuffer"
        messages = [f"integration_msg_{i}" for i in range(5)]
        input_file = self._create_input_file(messages)

        # Write via tracepipe
        result = clltk("tracepipe", "-b", buffer_name, input_file)
        self.assertEqual(result.returncode, 0, msg=result.stderr)

        # Decode the buffer
        trace_file = pathlib.Path(self.tmp_dir.name) / f"{buffer_name}.clltk_trace"
        result = clltk("decode", str(trace_file))
        self.assertEqual(result.returncode, 0, msg=result.stderr)

        # Verify all messages
        for msg in messages:
            self.assertIn(msg, result.stdout)

    def test_tracepipe_append_to_existing_buffer(self):
        """Test tracepipe appends to existing tracebuffer."""
        buffer_name = "AppendBuffer"

        # Create buffer and write initial message via trace command
        clltk("buffer", "-b", buffer_name, "-s", "8KB")
        clltk("trace", buffer_name, "initial_message")

        # Append via tracepipe
        append_messages = ["appended_msg_1", "appended_msg_2"]
        input_file = self._create_input_file(append_messages)
        result = clltk("tracepipe", "-b", buffer_name, input_file)
        self.assertEqual(result.returncode, 0, msg=result.stderr)

        # Decode and verify all messages
        decoded = self._decode_buffer(buffer_name)
        self.assertIn("initial_message", decoded)
        for msg in append_messages:
            self.assertIn(msg, decoded)

    def test_tracepipe_json_decode_verify(self):
        """Test JSON input through tracepipe with decode verification."""
        buffer_name = "JsonIntegration"
        json_messages = [
            {"message": "json_integration_1", "pid": 100},
            {"message": "json_integration_2", "tid": 200},
        ]
        json_lines = [json.dumps(obj) for obj in json_messages]
        input_file = self._create_input_file(json_lines)

        result = clltk("tracepipe", "-b", buffer_name, "--json", input_file)
        self.assertEqual(result.returncode, 0, msg=result.stderr)

        decoded = self._decode_buffer(buffer_name)
        self.assertIn("json_integration_1", decoded)
        self.assertIn("json_integration_2", decoded)


if __name__ == "__main__":
    unittest.main()
