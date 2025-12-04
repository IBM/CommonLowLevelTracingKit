#!/usr/bin/env python3
# Copyright (c) 2024, International Business Machines
# SPDX-License-Identifier: BSD-2-Clause-Patent

"""
CLLTK Live Command Basic Tests.

Basic CLI tests and streaming functionality tests.
"""

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

    def test_live_without_args_fails(self):
        """Test that live without required args fails."""
        import os
        # Ensure CLLTK_TRACING_PATH is not set, otherwise live will use it
        env = os.environ.copy()
        env.pop('CLLTK_TRACING_PATH', None)
        result = clltk("live", check=False, env=env)
        self.assertNotEqual(result.returncode, 0)


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
        result = clltk("tracebuffer", "--name", buffer_name, "--size", "4KB")
        self.assertEqual(result.returncode, 0, f"Failed to create tracebuffer: {result.stderr}")

        # Verify tracebuffer file exists
        trace_files = list(pathlib.Path(self.trace_path).glob("*.clltk_trace"))
        self.assertEqual(len(trace_files), 1, f"Expected 1 trace file, got {trace_files}")

        # Step 2-4: Use context manager
        with live_process(self.trace_path, order_delay=50, poll_interval=5) as live_proc:
            # Give live process time to start and discover tracebuffer
            time.sleep(0.3)

            # Step 3: Write tracepoints to the buffer
            for msg in test_messages:
                result = clltk("tracepoint", "--tb", buffer_name, "--msg", msg)
                self.assertEqual(result.returncode, 0, f"Failed to write tracepoint: {result.stderr}")
                time.sleep(0.05)  # Small delay between tracepoints

            # Give live time to process tracepoints
            time.sleep(0.5)

            # Step 4: Stop live process gracefully
            live_proc.send_signal(signal.SIGINT)
            stdout, stderr = live_proc.communicate(timeout=5)
            stdout_text = stdout.decode('utf-8')
            stderr_text = stderr.decode('utf-8')

            # Verify all test messages appear in output
            for msg in test_messages:
                self.assertIn(msg, stdout_text, 
                    f"Message '{msg}' not found in live output.\n"
                    f"stdout: {stdout_text}\nstderr: {stderr_text}")

            # Verify buffer name appears in output
            self.assertIn(buffer_name, stdout_text,
                f"Buffer name '{buffer_name}' not found in output")

    def test_multiple_tracepoints_ordered(self):
        """Test that multiple tracepoints are output in timestamp order."""
        buffer_name = "OrderTestBuffer"
        num_messages = 10

        # Create tracebuffer
        result = clltk("tracebuffer", "--name", buffer_name, "--size", "8KB")
        self.assertEqual(result.returncode, 0)

        with live_process(self.trace_path, order_delay=50, poll_interval=5) as live_proc:
            time.sleep(0.3)

            # Write numbered tracepoints
            for i in range(num_messages):
                msg = f"ordered_msg_{i:03d}"
                result = clltk("tracepoint", "--tb", buffer_name, "--msg", msg)
                self.assertEqual(result.returncode, 0)
                time.sleep(0.02)

            time.sleep(0.5)

            # Stop and get output
            live_proc.send_signal(signal.SIGINT)
            stdout, stderr = live_proc.communicate(timeout=5)
            stdout_text = stdout.decode('utf-8')

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
                self.assertLess(positions[j-1][1], positions[j][1],
                    f"Messages not in order: {positions[j-1]} should come before {positions[j]}")


if __name__ == '__main__':
    unittest.main()
