#!/usr/bin/env python3
# Copyright (c) 2024, International Business Machines
# SPDX-License-Identifier: BSD-2-Clause-Patent

"""
CLLTK Live Command Basic Tests.

Basic CLI tests and streaming functionality tests.
"""

import gzip
import os
import pathlib
import signal
import sys
import time
import unittest

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.realpath(__file__))))
sys.path.insert(0, os.path.dirname(os.path.realpath(__file__)))

from helpers.clltk_cmd import clltk
from test_live_base import LiveTestCase, get_clltk_path, live_process


class TestLiveCommandBasic(unittest.TestCase):
    """Basic CLI command tests for live subcommand."""

    def test_live_help(self):
        """Test that live --help shows usage information."""
        result = clltk("live", "--help")
        self.assertEqual(result.returncode, 0)
        self.assertIn("Monitor tracebuffers in real-time", result.stdout)
        self.assertIn("--buffer-size", result.stdout)
        self.assertIn("--order-delay", result.stdout)

    def test_live_without_args_succeeds(self):
        """Test that live without args succeeds (uses current directory)."""
        env = os.environ.copy()
        env.pop("CLLTK_TRACING_PATH", None)
        result = clltk("live", "--help", check=False, env=env)
        self.assertEqual(result.returncode, 0)


class TestLiveStreaming(LiveTestCase):
    """Test that tracepoints written to a buffer show up in live output."""

    def test_tracepoint_appears_in_live_output(self):
        """
        Key test: Write tracepoints to a buffer and verify they appear in live output.

        Steps:
        1. Create a tracebuffer
        2. Start clltk live in background
        3. Write tracepoints to the buffer
        4. Stop live and verify tracepoints are in output
        """
        buffer_name = "LiveTestBuffer"
        test_messages = [
            "live_test_message_001",
            "live_test_message_002",
            "live_test_message_003",
        ]

        # Step 1: Create tracebuffer
        result = clltk("buffer", "--buffer", buffer_name, "--size", "4KB")
        self.assertEqual(
            result.returncode, 0, f"Failed to create tracebuffer: {result.stderr}"
        )

        # Verify tracebuffer file exists
        trace_files = list(pathlib.Path(self.trace_path).glob("*.clltk_trace"))
        self.assertEqual(
            len(trace_files), 1, f"Expected 1 trace file, got {trace_files}"
        )

        # Step 2-4: Use context manager
        with live_process(
            self.trace_path, order_delay=50, poll_interval=5
        ) as live_proc:
            # Give live process time to start and discover tracebuffer
            time.sleep(0.3)

            # Step 3: Write tracepoints to the buffer
            for msg in test_messages:
                result = clltk("trace", "-b", buffer_name, msg)
                self.assertEqual(
                    result.returncode, 0, f"Failed to write tracepoint: {result.stderr}"
                )
                time.sleep(0.05)  # Small delay between tracepoints

            # Give live time to process tracepoints
            time.sleep(0.5)

            # Step 4: Stop live process gracefully
            live_proc.send_signal(signal.SIGINT)
            stdout, stderr = live_proc.communicate(timeout=5)
            stdout_text = stdout.decode("utf-8")
            stderr_text = stderr.decode("utf-8")

            # Verify all test messages appear in output
            for msg in test_messages:
                self.assertIn(
                    msg,
                    stdout_text,
                    f"Message '{msg}' not found in live output.\n"
                    f"stdout: {stdout_text}\nstderr: {stderr_text}",
                )

            # Verify buffer name appears in output
            self.assertIn(
                buffer_name,
                stdout_text,
                f"Buffer name '{buffer_name}' not found in output",
            )

    def test_multiple_tracepoints_ordered(self):
        """Test that multiple tracepoints are output in timestamp order."""
        buffer_name = "OrderTestBuffer"
        num_messages = 10

        # Create tracebuffer
        result = clltk("buffer", "--buffer", buffer_name, "--size", "8KB")
        self.assertEqual(result.returncode, 0)

        with live_process(
            self.trace_path, order_delay=50, poll_interval=5
        ) as live_proc:
            time.sleep(0.3)

            # Write numbered tracepoints
            for i in range(num_messages):
                msg = f"ordered_msg_{i:03d}"
                result = clltk("trace", "-b", buffer_name, msg)
                self.assertEqual(result.returncode, 0)
                time.sleep(0.02)

            time.sleep(0.5)

            # Stop and get output
            live_proc.send_signal(signal.SIGINT)
            stdout, stderr = live_proc.communicate(timeout=5)
            stdout_text = stdout.decode("utf-8")

            # Verify all messages present
            for i in range(num_messages):
                msg = f"ordered_msg_{i:03d}"
                self.assertIn(msg, stdout_text, f"Message '{msg}' not found")

            # Verify order: find positions of each message
            positions = []
            for i in range(num_messages):
                msg = f"ordered_msg_{i:03d}"
                pos = stdout_text.find(msg)
                if pos >= 0:
                    positions.append((i, pos))

            # Check that positions are increasing (messages in order)
            for j in range(1, len(positions)):
                self.assertLess(
                    positions[j - 1][1],
                    positions[j][1],
                    f"Messages not in order: {positions[j - 1]} should come before {positions[j]}",
                )


class TestLiveCompression(LiveTestCase):
    """Test compression option for live command."""

    def test_compress_help_shows_option(self):
        """Test that --compress option is shown in help."""
        result = clltk("live", "--help")
        self.assertEqual(result.returncode, 0)
        self.assertIn("--compress", result.stdout)
        self.assertIn("-z", result.stdout)

    def test_compress_output_shows_option(self):
        """Test that --output option is shown in help."""
        result = clltk("live", "--help")
        self.assertEqual(result.returncode, 0)
        self.assertIn("--output", result.stdout)
        self.assertIn("-o", result.stdout)

    def test_compress_output_to_file(self):
        """Test --compress writes valid gzip file with live output."""
        buffer_name = "LiveCompressTest"
        test_messages = ["compress_msg_001", "compress_msg_002", "compress_msg_003"]

        # Create tracebuffer
        result = clltk("buffer", "--buffer", buffer_name, "--size", "4KB")
        self.assertEqual(
            result.returncode, 0, f"Failed to create tracebuffer: {result.stderr}"
        )

        output_file = pathlib.Path(self.trace_path) / "live_output.gz"

        with live_process(
            self.trace_path,
            order_delay=50,
            poll_interval=5,
            extra_args=["-z", "-o", str(output_file)],
        ) as live_proc:
            # Give live process time to start
            time.sleep(0.3)

            # Write tracepoints
            for msg in test_messages:
                result = clltk("trace", "-b", buffer_name, msg)
                self.assertEqual(
                    result.returncode, 0, f"Failed to write tracepoint: {result.stderr}"
                )
                time.sleep(0.05)

            # Give live time to process and write
            time.sleep(0.5)

            # Stop live process
            live_proc.send_signal(signal.SIGINT)
            live_proc.communicate(timeout=5)

        # Verify the file exists and is valid gzip
        self.assertTrue(output_file.exists(), "Compressed output file was not created")

        # Decompress and verify content
        with gzip.open(output_file, "rt") as f:
            content = f.read()

        self.assertIn(buffer_name, content)
        for msg in test_messages:
            self.assertIn(
                msg, content, f"Message '{msg}' not found in compressed output"
            )

    def test_compress_with_json(self):
        """Test --compress works with --json for live output."""
        buffer_name = "LiveCompressJson"

        # Create tracebuffer
        result = clltk("buffer", "--buffer", buffer_name, "--size", "4KB")
        self.assertEqual(result.returncode, 0)

        output_file = pathlib.Path(self.trace_path) / "live_json.gz"

        with live_process(
            self.trace_path,
            order_delay=50,
            poll_interval=5,
            extra_args=["-z", "-j", "-o", str(output_file)],
        ) as live_proc:
            time.sleep(0.3)

            result = clltk("trace", "-b", buffer_name, "json_compress_test")
            self.assertEqual(result.returncode, 0)

            time.sleep(0.5)

            live_proc.send_signal(signal.SIGINT)
            live_proc.communicate(timeout=5)

        # Verify JSON output in compressed file
        self.assertTrue(output_file.exists())

        with gzip.open(output_file, "rt") as f:
            content = f.read()

        # Should contain JSON with tracebuffer field
        self.assertIn('"tracebuffer"', content)
        self.assertIn(buffer_name, content)


if __name__ == "__main__":
    unittest.main()
