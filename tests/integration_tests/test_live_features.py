#!/usr/bin/env python3
# Copyright (c) 2024, International Business Machines
# SPDX-License-Identifier: BSD-2-Clause-Patent

"""
CLLTK Live Command Feature Tests.

Tests for summary, multiple buffers, filter, and continuous writing.
"""

import os
import pathlib
import signal
import subprocess
import sys
import time
import unittest

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.realpath(__file__))))
sys.path.insert(0, os.path.dirname(os.path.realpath(__file__)))

from helpers.clltk_cmd import clltk
from test_live_base import LiveTestCase, get_clltk_path, live_process


class TestLiveWithSummary(LiveTestCase):
    """Test the --summary flag output."""

    def test_summary_output(self):
        """Test that --summary shows statistics."""
        buffer_name = "SummaryTestBuffer"

        # Create tracebuffer and write some tracepoints
        clltk("buffer", "--buffer", buffer_name, "--size", "4KB")

        with live_process(
            self.trace_path, order_delay=50, poll_interval=5, extra_args=["--summary"]
        ) as live_proc:
            time.sleep(0.3)

            # Write some tracepoints
            for i in range(5):
                clltk(
                    "trace", "--buffer", buffer_name, "--message", f"summary_test_{i}"
                )
                time.sleep(0.02)

            time.sleep(0.5)

            live_proc.send_signal(signal.SIGINT)
            stdout, stderr = live_proc.communicate(timeout=5)
            stderr_text = stderr.decode("utf-8")

            # Summary goes to stderr
            self.assertIn(
                "Live Decoder Summary",
                stderr_text,
                f"Summary header not found in stderr: {stderr_text}",
            )
            self.assertIn("Tracepoints read", stderr_text)
            self.assertIn("Tracepoints output", stderr_text)


class TestLiveMultipleBuffers(LiveTestCase):
    """Test live streaming with multiple tracebuffers."""

    def test_multiple_buffers_all_visible(self):
        """Test that tracepoints from multiple buffers all appear in output."""
        buffer_names = ["BufferAlpha", "BufferBeta", "BufferGamma"]

        # Create all tracebuffers
        for name in buffer_names:
            result = clltk("buffer", "--buffer", name, "--size", "4KB")
            self.assertEqual(result.returncode, 0)

        # Verify all trace files exist
        trace_files = list(pathlib.Path(self.trace_path).glob("*.clltk_trace"))
        self.assertEqual(len(trace_files), 3)

        with live_process(
            self.trace_path, order_delay=50, poll_interval=5
        ) as live_proc:
            time.sleep(0.3)

            # Write unique messages to each buffer
            for name in buffer_names:
                msg = f"message_from_{name}"
                result = clltk("trace", "--buffer", name, "--message", msg)
                self.assertEqual(result.returncode, 0)
                time.sleep(0.05)

            time.sleep(0.5)

            live_proc.send_signal(signal.SIGINT)
            stdout, stderr = live_proc.communicate(timeout=5)
            stdout_text = stdout.decode("utf-8")

            # Verify messages from ALL buffers appear
            for name in buffer_names:
                msg = f"message_from_{name}"
                self.assertIn(
                    msg, stdout_text, f"Message from '{name}' not found in output"
                )
                self.assertIn(
                    name, stdout_text, f"Buffer name '{name}' not found in output"
                )


class TestLiveTraceBufferFilter(LiveTestCase):
    """Test the --tracebuffer-filter regex option."""

    def test_filter_includes_matching_buffer(self):
        """Test that filter includes only matching buffers."""
        # Create buffers with different naming patterns
        clltk("buffer", "--buffer", "TestBufferOne", "--size", "4KB")
        clltk("buffer", "--buffer", "TestBufferTwo", "--size", "4KB")
        clltk("buffer", "--buffer", "OtherBuffer", "--size", "4KB")

        with live_process(
            self.trace_path,
            order_delay=50,
            poll_interval=5,
            extra_args=["--filter", "^TestBuffer.*"],
        ) as live_proc:
            time.sleep(0.3)

            # Write to all buffers
            clltk("trace", "--buffer", "TestBufferOne", "--message", "msg_test_one")
            clltk("trace", "--buffer", "TestBufferTwo", "--message", "msg_test_two")
            clltk("trace", "--buffer", "OtherBuffer", "--message", "msg_other")
            time.sleep(0.1)

            time.sleep(0.5)

            live_proc.send_signal(signal.SIGINT)
            stdout, stderr = live_proc.communicate(timeout=5)
            stdout_text = stdout.decode("utf-8")

            # Should see TestBuffer messages
            self.assertIn("msg_test_one", stdout_text)
            self.assertIn("msg_test_two", stdout_text)

            # Should NOT see OtherBuffer messages
            self.assertNotIn("msg_other", stdout_text)
            self.assertNotIn("OtherBuffer", stdout_text)


class TestLiveContinuousWriting(LiveTestCase):
    """Test live streaming while continuously writing tracepoints."""

    def test_continuous_write_and_read(self):
        """Test that live can keep up with continuous tracepoint writing."""
        buffer_name = "ContinuousBuffer"
        num_messages = 20

        clltk("buffer", "--buffer", buffer_name, "--size", "16KB")

        with live_process(
            self.trace_path, order_delay=30, poll_interval=5
        ) as live_proc:
            time.sleep(0.2)

            # Write messages rapidly
            for i in range(num_messages):
                msg = f"continuous_{i:04d}"
                clltk("trace", "--buffer", buffer_name, "--message", msg)
                # No sleep - write as fast as possible

            # Give time for processing
            time.sleep(0.8)

            live_proc.send_signal(signal.SIGINT)
            stdout, stderr = live_proc.communicate(timeout=5)
            stdout_text = stdout.decode("utf-8")

            # Count how many messages made it through
            found_count = 0
            for i in range(num_messages):
                msg = f"continuous_{i:04d}"
                if msg in stdout_text:
                    found_count += 1

            # We should see most or all messages
            self.assertGreaterEqual(
                found_count,
                num_messages - 2,
                f"Only found {found_count}/{num_messages} messages in output",
            )


if __name__ == "__main__":
    unittest.main()
