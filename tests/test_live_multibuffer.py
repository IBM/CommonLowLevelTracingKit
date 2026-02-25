#!/usr/bin/env python3
# Copyright (c) 2024, International Business Machines
# SPDX-License-Identifier: BSD-2-Clause-Patent

"""
CLLTK Live Command Multi-Buffer Tests.

Advanced tests for live decoder with multiple tracebuffers.
"""

import fcntl
import os
import select
import signal
import subprocess
import sys
import time
import unittest

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.realpath(__file__))))
sys.path.insert(0, os.path.dirname(os.path.realpath(__file__)))

from helpers.base import is_asan_build
from helpers.clltk_cmd import clltk
from test_live_base import LiveTestCase, get_clltk_path, run_live_with_timeout


class TestLiveMultiBufferScenarios(LiveTestCase):
    """
    Advanced tests for live decoder with multiple tracebuffers.
    """

    def test_multiple_buffers_preexisting_and_new_tracepoints(self):
        """
        Test with multiple buffers, each having pre-existing and new tracepoints.
        """
        buffer_names = ["MultiBufA", "MultiBufB", "MultiBufC"]
        clltk_path = get_clltk_path()

        all_pre_messages = {}

        # Create all buffers and write pre-existing tracepoints
        for buf_name in buffer_names:
            clltk("buffer", "--buffer", buf_name, "--size", "4KB")

            pre_msgs = [f"{buf_name}_pre_{i}" for i in range(2)]
            all_pre_messages[buf_name] = pre_msgs

            for msg in pre_msgs:
                result = subprocess.run(
                    [str(clltk_path), "trace", "-b", buf_name, msg],
                    env=os.environ.copy(),
                    capture_output=True,
                )
                self.assertEqual(result.returncode, 0)

        # Run decoder
        result = run_live_with_timeout(
            self.trace_path, timeout_seconds=3, order_delay=50, poll_interval=5
        )

        stdout_text = result.stdout.decode("utf-8")

        for buf_name, pre_msgs in all_pre_messages.items():
            for msg in pre_msgs:
                self.assertIn(
                    msg,
                    stdout_text,
                    f"Pre-existing message '{msg}' from buffer '{buf_name}' not found.\n"
                    f"Output sample: {stdout_text[:500]}...",
                )

    def test_timestamp_ordering_across_multiple_buffers(self):
        """
        Test that tracepoints from multiple buffers are output in timestamp order.
        """
        clltk_path = get_clltk_path()

        # Create two buffers
        clltk("buffer", "--buffer", "OrderBufA", "--size", "4KB")
        clltk("buffer", "--buffer", "OrderBufB", "--size", "4KB")

        # Write interleaved tracepoints with delays to ensure timestamp ordering
        messages_in_order = []
        for i in range(6):
            buf = "OrderBufA" if i % 2 == 0 else "OrderBufB"
            msg = f"order_test_{i:02d}_{buf}"
            messages_in_order.append(msg)

            subprocess.run(
                [str(clltk_path), "trace", "-b", buf, msg],
                env=os.environ.copy(),
                capture_output=True,
            )
            time.sleep(0.03)  # Ensure distinct timestamps

        # Run decoder
        result = run_live_with_timeout(
            self.trace_path, timeout_seconds=2, order_delay=30, poll_interval=5
        )

        stdout_text = result.stdout.decode("utf-8")

        # Verify all messages present
        for msg in messages_in_order:
            self.assertIn(msg, stdout_text, f"Message '{msg}' not found")

        # Verify ordering
        positions = []
        for msg in messages_in_order:
            pos = stdout_text.find(msg)
            positions.append((msg, pos))

        for i in range(1, len(positions)):
            self.assertLess(
                positions[i - 1][1],
                positions[i][1],
                f"Messages out of order: '{positions[i - 1][0]}' should appear before '{positions[i][0]}'",
            )

    def test_empty_buffer_then_write(self):
        """
        Test decoder handles initially empty buffer, then receives tracepoints.
        """
        buffer_name = "EmptyThenWriteBuffer"
        clltk_path = get_clltk_path()

        # Create empty buffer
        clltk("buffer", "--buffer", buffer_name, "--size", "4KB")

        # Start decoder on empty buffer
        cmd = []
        if not is_asan_build():
            cmd.extend(["stdbuf", "-oL"])
        cmd.extend(
            [
                str(clltk_path),
                "live",
                self.trace_path,
                "--order-delay",
                "30",
                "--poll-interval",
                "5",
            ]
        )
        decoder_proc = subprocess.Popen(
            cmd,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            env=os.environ.copy(),
        )

        try:
            fd = decoder_proc.stdout.fileno()
            flags = fcntl.fcntl(fd, fcntl.F_GETFL)
            fcntl.fcntl(fd, fcntl.F_SETFL, flags | os.O_NONBLOCK)

            # Wait for decoder to start (should handle empty buffer)
            time.sleep(0.5)

            # Now write tracepoints
            test_messages = [f"delayed_write_{i}" for i in range(3)]
            for msg in test_messages:
                subprocess.run(
                    [str(clltk_path), "trace", "-b", buffer_name, msg],
                    env=os.environ.copy(),
                    capture_output=True,
                )
                time.sleep(0.05)

            time.sleep(0.5)

            # Collect output
            collected = b""
            for _ in range(20):
                ready, _, _ = select.select([decoder_proc.stdout], [], [], 0.1)
                if ready:
                    try:
                        chunk = decoder_proc.stdout.read(4096)
                        if chunk:
                            collected += chunk
                    except BlockingIOError:
                        pass

            # Send SIGINT and get remaining
            decoder_proc.send_signal(signal.SIGINT)
            remaining, _ = decoder_proc.communicate(timeout=2)
            collected += remaining

            stdout_text = collected.decode("utf-8")

            for msg in test_messages:
                self.assertIn(
                    msg,
                    stdout_text,
                    f"Message '{msg}' not found. Decoder may not handle initially empty buffers.\n"
                    f"Output: {stdout_text[:500]}...",
                )

        except subprocess.TimeoutExpired:
            decoder_proc.kill()
            decoder_proc.wait()
            self.fail("Decoder timed out")
        except Exception as e:
            if decoder_proc.poll() is None:
                decoder_proc.kill()
                decoder_proc.wait()
            raise e


if __name__ == "__main__":
    unittest.main()
