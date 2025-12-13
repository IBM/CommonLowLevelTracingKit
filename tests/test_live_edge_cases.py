#!/usr/bin/env python3
# Copyright (c) 2024, International Business Machines
# SPDX-License-Identifier: BSD-2-Clause-Patent

"""
CLLTK Live Command Edge Case Tests.

Tests for edge cases and error handling.
"""

import os
import subprocess
import sys
import threading
import time
import unittest

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.realpath(__file__))))
sys.path.insert(0, os.path.dirname(os.path.realpath(__file__)))

from helpers.clltk_cmd import clltk
from test_live_base import LiveTestCase, get_clltk_path, run_live_with_timeout


class TestLiveEdgeCases(LiveTestCase):
    """
    Edge case tests for the live decoder.
    """

    def test_rapid_burst_from_multiple_writers(self):
        """
        Test decoder handles rapid burst of tracepoints from multiple concurrent writers.
        """
        buffer_name = "BurstTestBuffer"
        clltk_path = get_clltk_path()
        num_writers = 3
        messages_per_writer = 5

        clltk("buffer", "--buffer", buffer_name, "--size", "32KB")

        all_messages = []
        writer_errors = []
        lock = threading.Lock()

        def writer_func(writer_id):
            try:
                for i in range(messages_per_writer):
                    msg = f"burst_w{writer_id}_m{i}"
                    with lock:
                        all_messages.append(msg)
                    result = subprocess.run(
                        [str(clltk_path), 'trace', '-b', buffer_name, msg],
                        env=os.environ.copy(), capture_output=True
                    )
                    if result.returncode != 0:
                        with lock:
                            writer_errors.append(
                                f"Writer {writer_id} failed: {result.stderr}"
                            )
            except Exception as e:
                with lock:
                    writer_errors.append(str(e))

        # Start all writers concurrently
        writers = [
            threading.Thread(target=writer_func, args=(i,)) for i in range(num_writers)
        ]
        for w in writers:
            w.start()
        for w in writers:
            w.join(timeout=10)

        self.assertEqual(len(writer_errors), 0, f"Writer errors: {writer_errors}")

        # Run decoder
        result = run_live_with_timeout(
            self.trace_path, timeout_seconds=2, order_delay=50, poll_interval=5
        )

        stdout_text = result.stdout.decode("utf-8")

        # Count found messages
        found = sum(1 for msg in all_messages if msg in stdout_text)
        total = len(all_messages)

        self.assertGreaterEqual(
            found,
            total - 1,
            f"Only {found}/{total} messages found from burst write.\n"
            f"Output sample: {stdout_text[:1000]}...",
        )

    def test_large_message_content(self):
        """
        Test decoder handles tracepoints with larger message content.
        """
        buffer_name = "LargeMsgBuffer"
        clltk_path = get_clltk_path()

        clltk("buffer", "--buffer", buffer_name, "--size", "16KB")

        # Write messages of various sizes
        test_cases = [
            ("small", "S" * 10),
            ("medium", "M" * 100),
            ("larger", "L" * 500),
        ]

        for name, content in test_cases:
            msg = f"{name}:{content}"
            subprocess.run(
                [str(clltk_path), 'trace', '-b', buffer_name, msg],
                env=os.environ.copy(), capture_output=True
            )
            time.sleep(0.02)

        # Run decoder
        result = run_live_with_timeout(
            self.trace_path, timeout_seconds=2, order_delay=50, poll_interval=5
        )

        stdout_text = result.stdout.decode("utf-8")

        for name, content in test_cases:
            self.assertIn(name, stdout_text, f"Message with {name} content not found")

    def test_special_characters_in_message(self):
        """
        Test decoder handles tracepoints with special characters.
        """
        buffer_name = "SpecialCharBuffer"
        clltk_path = get_clltk_path()

        clltk("buffer", "--buffer", buffer_name, "--size", "8KB")

        # Messages with various special characters (that are safe for command line)
        test_messages = [
            "msg_with_numbers_12345",
            "msg_with_underscore_test",
            "msg-with-dashes",
            "msg.with.dots",
        ]

        for msg in test_messages:
            subprocess.run(
                [str(clltk_path), 'trace', '-b', buffer_name, msg],
                env=os.environ.copy(), capture_output=True
            )

        result = run_live_with_timeout(
            self.trace_path, timeout_seconds=2, order_delay=50, poll_interval=5
        )

        stdout_text = result.stdout.decode("utf-8")

        for msg in test_messages:
            self.assertIn(
                msg, stdout_text, f"Message '{msg}' with special chars not found"
            )

    def test_decoder_on_nonexistent_path_reports_error(self):
        """
        Test that decoder reports error on non-existent path.
        """
        clltk_path = get_clltk_path()

        result = subprocess.run(
            [str(clltk_path), "live", "/nonexistent/path/to/traces"],
            capture_output=True,
            env=os.environ.copy(),
        )

        # Should report invalid path
        stderr_text = result.stderr.decode("utf-8")
        self.assertIn(
            "Invalid input path",
            stderr_text,
            f"Expected 'Invalid input path' error message, got: {stderr_text}",
        )

    def test_decoder_on_empty_directory(self):
        """
        Test decoder handles empty directory (no tracebuffers).
        """
        clltk_path = get_clltk_path()

        # trace_path exists but has no tracebuffers
        result = subprocess.run(
            ["timeout", "1", str(clltk_path), "live", self.trace_path],
            capture_output=True,
            env=os.environ.copy(),
        )

        stderr_text = result.stderr.decode("utf-8")
        # Should report no tracebuffers found
        self.assertIn(
            "No tracebuffers found",
            stderr_text,
            f"Expected 'No tracebuffers found' message, got: {stderr_text}",
        )


if __name__ == "__main__":
    unittest.main()
