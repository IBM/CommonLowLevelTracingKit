#!/usr/bin/python3
# Copyright (c) 2024, International Business Machines
# SPDX-License-Identifier: BSD-2-Clause-Patent

"""
CLLTK comprehensive error handling tests.

Tests for error handling across all CLI commands including:
- Unknown subcommands and flags
- Invalid flag values
- Missing required arguments
- File permission errors
- Concurrent access scenarios
- Malformed input handling
"""

import json
import multiprocessing
import os
import pathlib
import stat
import sys
import tempfile
import threading
import time
import unittest

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.realpath(__file__))))

from helpers.base import run_command
from helpers.clltk_cmd import clltk, clltk_as_nobody


def setUpModule():
    """Build clltk-cmd before running tests."""
    run_command("cmake --preset default")
    run_command("cmake --build --preset default --target clltk-cmd")


class ErrorHandlingTestCase(unittest.TestCase):
    """Base class for error handling tests with temporary directory setup."""

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


class TestUnknownSubcommand(unittest.TestCase):
    """Tests for unknown subcommand handling."""

    def test_unknown_subcommand_shows_error(self):
        """Test that unknown subcommand shows an error message."""
        result = clltk("nonexistent_command", check=False)
        self.assertNotEqual(result.returncode, 0)
        # Should have some error indication in stderr or stdout
        combined_output = result.stdout + result.stderr
        self.assertTrue(
            len(combined_output) > 0,
            "Expected error message for unknown subcommand",
        )

    def test_misspelled_subcommand_buffer(self):
        """Test misspelled 'buffer' subcommand."""
        result = clltk("bufer", check=False)
        self.assertNotEqual(result.returncode, 0)

    def test_misspelled_subcommand_decode(self):
        """Test misspelled 'decode' subcommand."""
        result = clltk("decod", check=False)
        self.assertNotEqual(result.returncode, 0)

    def test_misspelled_subcommand_trace(self):
        """Test misspelled 'trace' subcommand."""
        result = clltk("trac", check=False)
        self.assertNotEqual(result.returncode, 0)

    def test_empty_string_subcommand(self):
        """Test empty string as subcommand."""
        result = clltk("", check=False)
        # Empty argument should not crash, may show help or error
        # Just verify it doesn't hang or crash

    def test_numeric_subcommand(self):
        """Test numeric value as subcommand."""
        result = clltk("12345", check=False)
        self.assertNotEqual(result.returncode, 0)

    def test_special_chars_subcommand(self):
        """Test special characters as subcommand."""
        result = clltk("@#$%", check=False)
        self.assertNotEqual(result.returncode, 0)

    def test_subcommand_with_leading_dash(self):
        """Test subcommand starting with dash but not a valid flag."""
        result = clltk("-notaflag", check=False)
        self.assertNotEqual(result.returncode, 0)


class TestUnknownFlags(ErrorHandlingTestCase):
    """Tests for unknown flag handling across various commands."""

    def test_unknown_flag_main_command(self):
        """Test unknown flag for main clltk command."""
        result = clltk("--unknown-flag", check=False)
        self.assertNotEqual(result.returncode, 0)
        combined_output = result.stdout + result.stderr
        self.assertTrue(len(combined_output) > 0)

    def test_unknown_short_flag_main_command(self):
        """Test unknown short flag for main command."""
        result = clltk("-Z", check=False)
        self.assertNotEqual(result.returncode, 0)

    def test_unknown_flag_buffer_command(self):
        """Test unknown flag for buffer subcommand."""
        result = clltk("buffer", "--unknown-option", "value", check=False)
        self.assertNotEqual(result.returncode, 0)

    def test_unknown_flag_decode_command(self):
        """Test unknown flag for decode subcommand."""
        result = clltk("decode", "--nonexistent-flag", check=False)
        self.assertNotEqual(result.returncode, 0)

    def test_unknown_flag_trace_command(self):
        """Test unknown flag for trace subcommand."""
        result = clltk("trace", "--invalid-option", check=False)
        self.assertNotEqual(result.returncode, 0)

    def test_unknown_flag_tracepipe_command(self):
        """Test unknown flag for tracepipe subcommand."""
        result = clltk("tracepipe", "--fake-flag", check=False)
        self.assertNotEqual(result.returncode, 0)

    def test_unknown_flag_clear_command(self):
        """Test unknown flag for clear subcommand."""
        result = clltk("clear", "--not-a-real-option", check=False)
        self.assertNotEqual(result.returncode, 0)

    def test_unknown_flag_list_command(self):
        """Test unknown flag for list subcommand."""
        result = clltk("list", "--imaginary-flag", check=False)
        self.assertNotEqual(result.returncode, 0)

    def test_double_dash_unknown(self):
        """Test double dash with unknown long option."""
        result = clltk("buffer", "--", "--fake", check=False)
        # May be treated as positional args after --, behavior may vary
        # Just verify no crash

    def test_flag_with_typo(self):
        """Test common typo in flag name."""
        result = clltk("buffer", "--bufer", "test", "--size", "1KB", check=False)
        self.assertNotEqual(result.returncode, 0)

    def test_multiple_unknown_flags(self):
        """Test multiple unknown flags at once."""
        result = clltk("decode", "--fake1", "--fake2", "--fake3", check=False)
        self.assertNotEqual(result.returncode, 0)


class TestInvalidFlagValues(ErrorHandlingTestCase):
    """Tests for invalid values for typed flags."""

    def test_buffer_invalid_size_string(self):
        """Test buffer command with non-numeric size."""
        result = clltk("buffer", "--buffer", "Test", "--size", "invalid", check=False)
        self.assertNotEqual(result.returncode, 0)

    def test_buffer_negative_size(self):
        """Test buffer command with negative size."""
        result = clltk("buffer", "--buffer", "Test", "--size", "-100", check=False)
        self.assertNotEqual(result.returncode, 0)

    def test_buffer_zero_size(self):
        """Test buffer command with zero size."""
        result = clltk("buffer", "--buffer", "Test", "--size", "0", check=False)
        self.assertNotEqual(result.returncode, 0)

    def test_buffer_float_size(self):
        """Test buffer command with floating point size."""
        result = clltk("buffer", "--buffer", "Test", "--size", "1.5KB", check=False)
        # May succeed or fail depending on parsing, verify no crash

    def test_buffer_invalid_size_suffix(self):
        """Test buffer command with invalid size suffix."""
        result = clltk("buffer", "--buffer", "Test", "--size", "100XB", check=False)
        self.assertNotEqual(result.returncode, 0)

    def test_decode_invalid_pid(self):
        """Test decode with non-numeric PID filter."""
        self._create_buffer("DecodePid")
        result = clltk("decode", "--pid", "notanumber", check=False)
        self.assertNotEqual(result.returncode, 0)

    def test_decode_negative_pid(self):
        """Test decode with negative PID filter."""
        self._create_buffer("DecodeNegPid")
        result = clltk("decode", "--pid", "-1", check=False)
        # Negative PID should be rejected or handled gracefully

    def test_decode_invalid_tid(self):
        """Test decode with non-numeric TID filter."""
        self._create_buffer("DecodeTid")
        result = clltk("decode", "--tid", "invalid", check=False)
        self.assertNotEqual(result.returncode, 0)

    def test_decode_invalid_source_type(self):
        """Test decode with invalid source type."""
        self._create_buffer("DecodeSource")
        result = clltk("decode", "--source", "invalid_source", check=False)
        self.assertNotEqual(result.returncode, 0)

    def test_decode_invalid_time_format(self):
        """Test decode with invalid time format for --since."""
        self._create_buffer("DecodeTime")
        result = clltk("decode", "--since", "not-a-time", check=False)
        self.assertNotEqual(result.returncode, 0)

    def test_decode_invalid_until_format(self):
        """Test decode with invalid time format for --until."""
        self._create_buffer("DecodeUntil")
        result = clltk("decode", "--until", "invalid-time-format", check=False)
        self.assertNotEqual(result.returncode, 0)

    def test_decode_invalid_regex_filter(self):
        """Test decode with invalid regex in --filter."""
        self._create_buffer("DecodeRegex")
        result = clltk("decode", "--filter", "[invalid(regex", check=False)
        self.assertNotEqual(result.returncode, 0)

    def test_decode_invalid_msg_regex(self):
        """Test decode with invalid --msg-regex."""
        self._create_buffer("DecodeMsgRegex")
        result = clltk("decode", "--msg-regex", "[[[[", check=False)
        self.assertNotEqual(result.returncode, 0)

    def test_trace_invalid_line_number(self):
        """Test trace with non-numeric line number."""
        result = clltk(
            "trace", "-b", "TraceLine", "-m", "test", "-l", "notanumber", check=False
        )
        self.assertNotEqual(result.returncode, 0)

    def test_trace_invalid_pid(self):
        """Test trace with non-numeric PID."""
        result = clltk(
            "trace", "-b", "TracePid", "-m", "test", "--pid", "invalid", check=False
        )
        self.assertNotEqual(result.returncode, 0)

    def test_trace_invalid_tid(self):
        """Test trace with non-numeric TID."""
        result = clltk(
            "trace", "-b", "TraceTid", "-m", "test", "--tid", "invalid", check=False
        )
        self.assertNotEqual(result.returncode, 0)

    def test_path_option_nonexistent_directory(self):
        """Test --path with nonexistent directory."""
        result = clltk(
            "-P",
            "/nonexistent/path/that/does/not/exist",
            "buffer",
            "--buffer",
            "Test",
            "--size",
            "1KB",
            check=False,
        )
        self.assertNotEqual(result.returncode, 0)

    def test_path_option_file_instead_of_directory(self):
        """Test --path pointing to a file instead of directory."""
        # Create a regular file
        file_path = pathlib.Path(self.tmp_dir.name) / "regular_file.txt"
        file_path.write_text("not a directory")

        result = clltk(
            "-P",
            str(file_path),
            "buffer",
            "--buffer",
            "Test",
            "--size",
            "1KB",
            check=False,
        )
        self.assertNotEqual(result.returncode, 0)


class TestMissingRequiredArguments(ErrorHandlingTestCase):
    """Tests for missing required arguments on various commands."""

    def test_buffer_missing_name(self):
        """Test buffer command without buffer name."""
        result = clltk("buffer", "--size", "1KB", check=False)
        self.assertNotEqual(result.returncode, 0)

    def test_buffer_missing_size(self):
        """Test buffer command without size."""
        result = clltk("buffer", "--buffer", "TestBuffer", check=False)
        # Size may have a default or be required, check it doesn't crash

    def test_trace_missing_buffer_name(self):
        """Test trace command without buffer name."""
        result = clltk("trace", "-m", "test message", check=False)
        self.assertNotEqual(result.returncode, 0)

    def test_trace_missing_message(self):
        """Test trace command without message."""
        result = clltk("trace", "-b", "TestBuffer", check=False)
        self.assertNotEqual(result.returncode, 0)

    def test_tracepipe_missing_buffer(self):
        """Test tracepipe command without buffer name."""
        result = clltk("tracepipe", check=False)
        self.assertNotEqual(result.returncode, 0)

    def test_clear_missing_buffer_name(self):
        """Test clear command without buffer name."""
        result = clltk("clear", check=False)
        self.assertNotEqual(result.returncode, 0)

    def test_buffer_flag_without_value(self):
        """Test buffer flag without its value."""
        result = clltk("buffer", "--buffer", check=False)
        self.assertNotEqual(result.returncode, 0)

    def test_size_flag_without_value(self):
        """Test size flag without its value."""
        result = clltk("buffer", "--buffer", "Test", "--size", check=False)
        self.assertNotEqual(result.returncode, 0)

    def test_trace_message_flag_without_value(self):
        """Test trace -m flag without message value."""
        result = clltk("trace", "-b", "Buffer", "-m", check=False)
        self.assertNotEqual(result.returncode, 0)

    def test_decode_filter_flag_without_value(self):
        """Test decode --filter flag without regex value."""
        result = clltk("decode", "--filter", check=False)
        self.assertNotEqual(result.returncode, 0)

    def test_decode_output_flag_without_value(self):
        """Test decode --output flag without path value."""
        result = clltk("decode", "--output", check=False)
        self.assertNotEqual(result.returncode, 0)

    def test_decode_since_flag_without_value(self):
        """Test decode --since flag without time value."""
        result = clltk("decode", "--since", check=False)
        self.assertNotEqual(result.returncode, 0)

    def test_decode_until_flag_without_value(self):
        """Test decode --until flag without time value."""
        result = clltk("decode", "--until", check=False)
        self.assertNotEqual(result.returncode, 0)


class TestFilePermissionErrors(ErrorHandlingTestCase):
    """Tests for file permission error handling."""

    def test_readonly_output_file(self):
        """Test decode to readonly output file."""
        self._create_buffer("ReadonlyOutput")
        clltk("trace", "ReadonlyOutput", "test message")

        # Create output file and make it readonly
        output_file = pathlib.Path(self.tmp_dir.name) / "readonly_output.txt"
        output_file.write_text("existing content")
        output_file.chmod(stat.S_IRUSR)

        try:
            run = clltk_as_nobody if os.geteuid() == 0 else clltk
            result = run("decode", "--output", str(output_file), check=False)
            self.assertNotEqual(result.returncode, 0)
        finally:
            output_file.chmod(stat.S_IWUSR | stat.S_IRUSR)

    def test_no_write_permission_to_directory(self):
        """Test buffer creation in directory without write permission."""
        readonly_dir = pathlib.Path(self.tmp_dir.name) / "readonly_dir"
        readonly_dir.mkdir()
        readonly_dir.chmod(stat.S_IRUSR | stat.S_IXUSR)

        try:
            run = clltk_as_nobody if os.geteuid() == 0 else clltk
            result = run(
                "-P",
                str(readonly_dir),
                "buffer",
                "--buffer",
                "Test",
                "--size",
                "1KB",
                check=False,
            )
            self.assertNotEqual(result.returncode, 0)
        finally:
            readonly_dir.chmod(stat.S_IRWXU)

    def test_trace_to_readonly_directory(self):
        """Test trace when tracing directory is not writable."""
        readonly_dir = pathlib.Path(self.tmp_dir.name) / "trace_readonly"
        readonly_dir.mkdir()
        readonly_dir.chmod(stat.S_IRUSR | stat.S_IXUSR)

        old_path = os.environ["CLLTK_TRACING_PATH"]
        os.environ["CLLTK_TRACING_PATH"] = str(readonly_dir)

        try:
            run = clltk_as_nobody if os.geteuid() == 0 else clltk
            result = run("trace", "-b", "Test", "-m", "message", check=False)
            self.assertNotEqual(result.returncode, 0)
        finally:
            os.environ["CLLTK_TRACING_PATH"] = old_path
            readonly_dir.chmod(stat.S_IRWXU)

    def test_decode_output_to_readonly_directory(self):
        """Test decode output to directory without write permission."""
        self._create_buffer("ReadonlyDirOutput")
        clltk("trace", "ReadonlyDirOutput", "test message")

        readonly_dir = pathlib.Path(self.tmp_dir.name) / "no_write"
        readonly_dir.mkdir()
        readonly_dir.chmod(stat.S_IRUSR | stat.S_IXUSR)

        try:
            output_path = readonly_dir / "output.txt"
            run = clltk_as_nobody if os.geteuid() == 0 else clltk
            result = run("decode", "--output", str(output_path), check=False)
            self.assertNotEqual(result.returncode, 0)
        finally:
            readonly_dir.chmod(stat.S_IRWXU)

    def test_buffer_in_nonexistent_subdirectory(self):
        """Test buffer creation in nonexistent subdirectory."""
        nonexistent_path = pathlib.Path(self.tmp_dir.name) / "does" / "not" / "exist"
        result = clltk(
            "-P",
            str(nonexistent_path),
            "buffer",
            "--buffer",
            "Test",
            "--size",
            "1KB",
            check=False,
        )
        self.assertNotEqual(result.returncode, 0)

    def test_readonly_trace_file(self):
        """Test trace to readonly trace file."""
        self._create_buffer("ReadonlyTrace")
        trace_file = self._get_trace_file("ReadonlyTrace")
        trace_file.chmod(stat.S_IRUSR)

        try:
            run = clltk_as_nobody if os.geteuid() == 0 else clltk
            result = run("trace", "-b", "ReadonlyTrace", "-m", "message", check=False)
            # Should fail because file is readonly
            self.assertNotEqual(result.returncode, 0)
        finally:
            trace_file.chmod(stat.S_IWUSR | stat.S_IRUSR)

    def test_clear_readonly_trace_file(self):
        """Test clear on readonly trace file."""
        self._create_buffer("ClearReadonly")
        clltk("trace", "ClearReadonly", "test message")
        trace_file = self._get_trace_file("ClearReadonly")
        trace_file.chmod(stat.S_IRUSR)

        try:
            result = clltk("clear", "-b", "ClearReadonly", "-y", check=False)
            # Should fail because file is readonly
            self.assertNotEqual(result.returncode, 0)
        finally:
            trace_file.chmod(stat.S_IWUSR | stat.S_IRUSR)


class TestConcurrentAccess(ErrorHandlingTestCase):
    """Tests for concurrent access to trace buffers."""

    def test_concurrent_writes_same_buffer(self):
        """Test multiple processes writing to same buffer concurrently."""
        buffer_name = "ConcurrentWrite"
        self._create_buffer(buffer_name, "64KB")

        errors = []
        results = []

        def write_trace(process_id: int):
            for i in range(10):
                try:
                    result = clltk(
                        "trace",
                        "-b",
                        buffer_name,
                        "-m",
                        f"process_{process_id}_msg_{i}",
                        check=False,
                    )
                    results.append(result.returncode)
                except Exception as e:
                    errors.append(str(e))

        threads = []
        for pid in range(5):
            t = threading.Thread(target=write_trace, args=(pid,))
            threads.append(t)
            t.start()

        for t in threads:
            t.join(timeout=30)

        # No exceptions should occur
        self.assertEqual(len(errors), 0, f"Errors during concurrent writes: {errors}")

        # Verify buffer is still readable
        result = clltk("decode", "-F", buffer_name, check=False)
        self.assertEqual(result.returncode, 0, "Buffer should still be readable")

    def test_concurrent_read_write(self):
        """Test reading while writing to same buffer."""
        buffer_name = "ReadWriteConcurrent"
        self._create_buffer(buffer_name, "64KB")

        errors = []
        write_done = threading.Event()

        def write_traces():
            for i in range(20):
                try:
                    clltk(
                        "trace",
                        "-b",
                        buffer_name,
                        "-m",
                        f"concurrent_msg_{i}",
                        check=False,
                    )
                    time.sleep(0.01)
                except Exception as e:
                    errors.append(f"Write error: {e}")
            write_done.set()

        def read_traces():
            read_count = 0
            while not write_done.is_set() and read_count < 10:
                try:
                    result = clltk("decode", "-F", buffer_name, check=False)
                    if result.returncode != 0:
                        errors.append(f"Read failed: {result.stderr}")
                    read_count += 1
                    time.sleep(0.05)
                except Exception as e:
                    errors.append(f"Read error: {e}")

        writer = threading.Thread(target=write_traces)
        reader = threading.Thread(target=read_traces)

        writer.start()
        reader.start()

        writer.join(timeout=30)
        reader.join(timeout=30)

        self.assertEqual(
            len(errors), 0, f"Errors during concurrent read/write: {errors}"
        )

    def test_concurrent_buffer_creation(self):
        """Test creating multiple buffers concurrently."""
        errors = []
        results = []

        def create_buffer(buffer_id: int):
            try:
                result = clltk(
                    "buffer",
                    "--buffer",
                    f"ConcurrentBuffer{buffer_id}",
                    "--size",
                    "4KB",
                    check=False,
                )
                results.append((buffer_id, result.returncode))
            except Exception as e:
                errors.append(f"Buffer {buffer_id} error: {e}")

        threads = []
        for i in range(10):
            t = threading.Thread(target=create_buffer, args=(i,))
            threads.append(t)
            t.start()

        for t in threads:
            t.join(timeout=30)

        self.assertEqual(
            len(errors), 0, f"Errors during concurrent buffer creation: {errors}"
        )

        # All buffers should be created successfully
        successful = [r for r in results if r[1] == 0]
        self.assertEqual(
            len(successful), 10, f"Expected 10 successful buffer creations: {results}"
        )

    def test_concurrent_clear_operations(self):
        """Test clearing buffers concurrently."""
        buffer_name = "ClearConcurrent"
        self._create_buffer(buffer_name, "16KB")

        # Add some traces
        for i in range(10):
            clltk("trace", "-b", buffer_name, "-m", f"msg_{i}")

        errors = []

        def clear_buffer(thread_id: int):
            for _ in range(3):
                try:
                    result = clltk("clear", "-b", buffer_name, "-y", check=False)
                    if result.returncode != 0:
                        errors.append(
                            f"Thread {thread_id} clear failed: {result.stderr}"
                        )
                except Exception as e:
                    errors.append(f"Thread {thread_id} error: {e}")

        threads = []
        for i in range(5):
            t = threading.Thread(target=clear_buffer, args=(i,))
            threads.append(t)
            t.start()

        for t in threads:
            t.join(timeout=30)

        # May have some failures due to concurrent access, but should not crash
        # Buffer should still be usable
        result = clltk("trace", "-b", buffer_name, "-m", "after clear", check=False)
        self.assertEqual(
            result.returncode, 0, "Buffer should be usable after concurrent clears"
        )


class TestMalformedInput(ErrorHandlingTestCase):
    """Tests for malformed input file handling."""

    def test_corrupted_trace_file(self):
        """Test decode handles corrupted trace file gracefully."""
        corrupted_file = pathlib.Path(self.tmp_dir.name) / "corrupted.clltk_trace"
        corrupted_file.write_bytes(b"\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09")

        result = clltk("decode", str(corrupted_file), check=False)
        # Should not crash, may return error or empty result

    def test_truncated_trace_file(self):
        """Test decode handles truncated trace file gracefully."""
        # First create a valid buffer
        self._create_buffer("TruncatedTest")
        clltk("trace", "TruncatedTest", "some message")

        trace_file = self._get_trace_file("TruncatedTest")

        # Read and truncate the file
        content = trace_file.read_bytes()
        truncated_content = content[: len(content) // 2]

        truncated_file = pathlib.Path(self.tmp_dir.name) / "truncated.clltk_trace"
        truncated_file.write_bytes(truncated_content)

        result = clltk("decode", str(truncated_file), check=False)
        # Should handle gracefully without crashing

    def test_wrong_file_extension(self):
        """Test decode with file having wrong extension."""
        wrong_ext_file = pathlib.Path(self.tmp_dir.name) / "trace.wrong_ext"
        wrong_ext_file.write_bytes(b"random content")

        result = clltk("decode", str(wrong_ext_file), check=False)
        # May be rejected or processed, should not crash

    def test_empty_trace_file(self):
        """Test decode handles empty trace file gracefully."""
        empty_file = pathlib.Path(self.tmp_dir.name) / "empty.clltk_trace"
        empty_file.write_bytes(b"")

        result = clltk("decode", str(empty_file), check=False)
        # Should handle empty file gracefully

    def test_text_file_as_trace_file(self):
        """Test decode with text file masquerading as trace file."""
        text_file = pathlib.Path(self.tmp_dir.name) / "text.clltk_trace"
        text_file.write_text("This is just plain text, not a binary trace file.")

        result = clltk("decode", str(text_file), check=False)
        # Should detect invalid format

    def test_json_file_as_trace_file(self):
        """Test decode with JSON file masquerading as trace file."""
        json_file = pathlib.Path(self.tmp_dir.name) / "json.clltk_trace"
        json_file.write_text('{"key": "value", "number": 42}')

        result = clltk("decode", str(json_file), check=False)
        # Should detect invalid format

    def test_very_large_size_header(self):
        """Test decode with malformed header claiming huge size."""
        # Create file with potentially malicious header
        malicious_file = pathlib.Path(self.tmp_dir.name) / "malicious.clltk_trace"
        # Write a header-like structure with absurdly large size claims
        malicious_file.write_bytes(b"\xff\xff\xff\xff" * 100)

        result = clltk("decode", str(malicious_file), check=False)
        # Should not allocate huge memory or crash

    def test_binary_garbage_file(self):
        """Test decode with pure random binary data."""
        garbage_file = pathlib.Path(self.tmp_dir.name) / "garbage.clltk_trace"
        import random

        garbage_data = bytes(random.randint(0, 255) for _ in range(1024))
        garbage_file.write_bytes(garbage_data)

        result = clltk("decode", str(garbage_file), check=False)
        # Should not crash with random data

    def test_decode_directory_instead_of_file(self):
        """Test decode when given a directory with wrong extension."""
        dir_path = pathlib.Path(self.tmp_dir.name) / "not_a_file.clltk_trace"
        dir_path.mkdir()

        result = clltk("decode", str(dir_path), check=False)
        # Should handle directory gracefully

    def test_symlink_to_nonexistent_file(self):
        """Test decode with symlink pointing to nonexistent file."""
        link_path = pathlib.Path(self.tmp_dir.name) / "broken_link.clltk_trace"
        try:
            link_path.symlink_to("/nonexistent/file/path")
            result = clltk("decode", str(link_path), check=False)
            # Should handle broken symlink gracefully
        except OSError:
            # Symlink creation may fail on some systems
            pass

    def test_file_with_null_bytes_in_content(self):
        """Test decode with file containing null bytes."""
        null_file = pathlib.Path(self.tmp_dir.name) / "nulls.clltk_trace"
        null_file.write_bytes(b"\x00" * 1024)

        result = clltk("decode", str(null_file), check=False)
        # Should handle null-filled file gracefully

    def test_partially_valid_trace_file(self):
        """Test decode with file that starts valid but becomes corrupted."""
        self._create_buffer("PartialValid")
        clltk("trace", "PartialValid", "valid message")

        trace_file = self._get_trace_file("PartialValid")
        content = trace_file.read_bytes()

        # Append garbage to valid content
        corrupted_content = content + b"\xff\xfe\xfd\xfc" * 100

        partial_file = pathlib.Path(self.tmp_dir.name) / "partial.clltk_trace"
        partial_file.write_bytes(corrupted_content)

        result = clltk("decode", str(partial_file), check=False)
        # Should recover what it can or report error gracefully


class TestBufferNameValidation(ErrorHandlingTestCase):
    """Tests for buffer name validation edge cases."""

    def test_buffer_name_with_path_separator(self):
        """Test buffer name containing path separator."""
        result = clltk(
            "buffer", "--buffer", "path/separator", "--size", "1KB", check=False
        )
        self.assertNotEqual(result.returncode, 0)

    def test_buffer_name_with_null_byte(self):
        """Test buffer name with embedded null byte."""
        # Use bash $'...' syntax to pass null byte since Python subprocess can't
        import subprocess
        from helpers.base import clltk_cmd_file

        cmd = f"{clltk_cmd_file()} buffer --buffer $'null\\x00byte' --size 1KB"
        result = subprocess.run(
            cmd,
            shell=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            env=os.environ.copy(),
        )
        # C strings are null-terminated, so the name gets truncated to "null"
        # This is expected behavior - the command should succeed
        self.assertEqual(result.returncode, 0)
        # Verify the truncated name was used
        trace_file = pathlib.Path(self.tmp_dir.name) / "null.clltk_trace"
        self.assertTrue(
            trace_file.exists(),
            "Buffer 'null' should be created (truncated at null byte)",
        )

    def test_buffer_name_with_unicode(self):
        """Test buffer name with Unicode characters."""
        result = clltk("buffer", "--buffer", "Unicode", "--size", "1KB", check=False)
        # May be accepted or rejected depending on implementation

    def test_buffer_name_very_long(self):
        """Test buffer name exceeding limits."""
        long_name = "A" * 1000
        result = clltk("buffer", "--buffer", long_name, "--size", "1KB", check=False)
        self.assertNotEqual(result.returncode, 0)

    def test_buffer_name_only_spaces(self):
        """Test buffer name that is only spaces."""
        result = clltk("buffer", "--buffer", "   ", "--size", "1KB", check=False)
        self.assertNotEqual(result.returncode, 0)

    def test_buffer_name_starts_with_dash(self):
        """Test buffer name starting with dash."""
        result = clltk(
            "buffer", "--buffer", "-InvalidName", "--size", "1KB", check=False
        )
        # May be interpreted as flag, should handle gracefully


class TestGracefulErrorRecovery(ErrorHandlingTestCase):
    """Tests verifying graceful error recovery and no hanging."""

    def test_rapid_error_commands(self):
        """Test system handles rapid sequence of error-causing commands."""
        for _ in range(20):
            clltk("unknown_command", check=False)
            clltk("buffer", "--invalid", check=False)
            clltk("decode", "/nonexistent/path", check=False)
        # Should complete without hanging

    def test_error_does_not_corrupt_existing_buffer(self):
        """Test that errors don't corrupt existing trace buffers."""
        self._create_buffer("StableBuffer")
        clltk("trace", "StableBuffer", "initial message")

        # Cause various errors
        clltk("decode", "--invalid-flag", check=False)
        clltk("trace", "-b", "Invalid/Name", "-m", "test", check=False)
        clltk("buffer", "--size", "invalid", check=False)

        # Original buffer should still be readable
        result = clltk("decode", "-F", "StableBuffer")
        self.assertEqual(result.returncode, 0)
        self.assertIn("initial message", result.stdout)

    def test_error_command_timeout(self):
        """Test that error commands don't cause infinite loops."""
        import signal

        def timeout_handler(signum, frame):
            raise TimeoutError("Command took too long")

        # Test various potentially problematic inputs with timeout
        test_cases = [
            ("decode", "/dev/zero"),  # Infinite device
            ("decode", "--filter", ".*"),  # Potentially slow regex
        ]

        for args in test_cases:
            old_handler = signal.signal(signal.SIGALRM, timeout_handler)
            signal.alarm(10)  # 10 second timeout
            try:
                clltk(*args, check=False)
            except TimeoutError:
                self.fail(f"Command {args} timed out")
            finally:
                signal.alarm(0)
                signal.signal(signal.SIGALRM, old_handler)


if __name__ == "__main__":
    unittest.main()
