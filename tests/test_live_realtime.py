#!/usr/bin/env python3
# Copyright (c) 2024, International Business Machines
# SPDX-License-Identifier: BSD-2-Clause-Patent

"""
CLLTK Live Command Real-Time Tests.

Tests for real-time output and separate writer process scenarios.
"""

import fcntl
import os
import select
import signal
import subprocess
import sys
import threading
import time
import unittest

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.realpath(__file__))))
sys.path.insert(0, os.path.dirname(os.path.realpath(__file__)))

from helpers.base import is_asan_build
from helpers.clltk_cmd import clltk
from test_live_base import (
    LiveTestCase,
    get_clltk_path,
    live_process,
    run_live_with_timeout,
)


class TestLiveRealTimeOutput(LiveTestCase):
    """
    Test that tracepoints appear in REAL-TIME, not just after Ctrl+C.

    This test specifically catches the watermark bug where tracepoints
    were only flushed after finish() was called.
    """

    def test_existing_tracepoints_appear_before_sigint(self):
        """
        Test that existing tracepoints in a buffer appear BEFORE sending SIGINT.

        This catches the watermark bug where output only happened after finish().
        """
        buffer_name = "RealTimeTestBuffer"
        test_msg = "realtime_test_message_12345"

        # Step 1: Create tracebuffer and write tracepoints BEFORE starting live
        clltk("buffer", "--buffer", buffer_name, "--size", "4KB")
        clltk("trace", "-b", buffer_name, test_msg)

        # Step 2: Run live with timeout
        result = run_live_with_timeout(
            self.trace_path, timeout_seconds=1, order_delay=50, poll_interval=5
        )

        stdout_text = result.stdout.decode("utf-8")

        # Step 3: Verify the tracepoint appeared during the 1 second window
        self.assertIn(
            test_msg,
            stdout_text,
            f"Tracepoint did not appear in real-time output (before timeout killed process).\n"
            f"This indicates the watermark bug - output only happens after SIGINT.\n"
            f"stdout: {stdout_text}\nstderr: {result.stderr.decode('utf-8')}",
        )

    def test_new_tracepoints_appear_in_realtime(self):
        """
        Test that newly written tracepoints appear in real-time while live is running.
        """
        buffer_name = "StreamingTestBuffer"

        clltk("buffer", "--buffer", buffer_name, "--size", "4KB")

        with live_process(
            self.trace_path, order_delay=30, poll_interval=5
        ) as live_proc:
            time.sleep(0.2)  # Let live start

            # Write a tracepoint
            test_msg = "streaming_realtime_test_xyz"
            clltk("trace", "-b", buffer_name, test_msg)

            # Wait for it to be processed and output (order_delay + processing)
            time.sleep(0.3)

            # Now send SIGINT and collect output
            live_proc.send_signal(signal.SIGINT)
            stdout, stderr = live_proc.communicate(timeout=5)
            stdout_text = stdout.decode("utf-8")

            # The message should be there
            self.assertIn(test_msg, stdout_text)

            # Verify it appeared at least once
            lines = [l for l in stdout_text.split("\n") if test_msg in l]
            self.assertGreaterEqual(
                len(lines), 1, "Test message should appear at least once"
            )


class TestLiveWithSeparateWriterProcess(LiveTestCase):
    """
    Test live decoder with a separate writer process.
    """

    def test_decoder_receives_tracepoints_from_separate_writer_process(self):
        """
        End-to-end test: decoder in one subprocess, writer in another.
        """
        buffer_name = "E2ETestBuffer"
        test_messages = [f"e2e_test_msg_{i:03d}" for i in range(5)]

        # Step 1: Create tracebuffer
        clltk("buffer", "--buffer", buffer_name, "--size", "8KB")

        # Step 2: Create a writer script that runs as separate process
        clltk_path = get_clltk_path()
        writer_script = f"""
import subprocess
import time
import os

os.environ['CLLTK_TRACING_PATH'] = '{self.trace_path}'
clltk = '{clltk_path}'

messages = {test_messages!r}
for msg in messages:
    subprocess.run([clltk, 'trace', '-b', '{buffer_name}', msg],
                   env=os.environ, check=True)
    time.sleep(0.05)
"""
        # Run writer process
        writer_result = subprocess.run(
            ["python3", "-c", writer_script],
            capture_output=True,
            env=os.environ.copy(),
            timeout=10,
        )
        self.assertEqual(
            writer_result.returncode,
            0,
            f"Writer process failed: {writer_result.stderr.decode()}",
        )

        # Step 3: Run decoder with timeout
        result = run_live_with_timeout(
            self.trace_path, timeout_seconds=2, order_delay=50, poll_interval=5
        )

        stdout_text = result.stdout.decode("utf-8")

        # Verify all messages appeared
        found_count = sum(1 for msg in test_messages if msg in stdout_text)
        self.assertEqual(
            found_count,
            len(test_messages),
            f"Only {found_count}/{len(test_messages)} messages from writer process "
            f"found in decoder output.\nDecoder output: {stdout_text}",
        )

    def test_preexisting_and_new_tracepoints_from_writer_process(self):
        """
        Test that decoder sees both pre-existing tracepoints AND new ones from writer.
        """
        buffer_name = "PreExistAndNewBuffer"
        pre_messages = [f"pre_existing_msg_{i:03d}" for i in range(3)]
        post_messages = [f"post_start_msg_{i:03d}" for i in range(3)]
        all_messages = pre_messages + post_messages

        # Step 1: Create tracebuffer
        clltk("buffer", "--buffer", buffer_name, "--size", "8KB")

        clltk_path = get_clltk_path()

        # Step 2: Write some tracepoints BEFORE decoder starts
        for msg in pre_messages:
            result = subprocess.run(
                [str(clltk_path), "trace", "-b", buffer_name, msg],
                env=os.environ.copy(),
                capture_output=True,
            )
            self.assertEqual(
                result.returncode,
                0,
                f"Failed to write pre-existing tracepoint: {result.stderr.decode()}",
            )

        # Step 3: Start decoder with piped stdout
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
            # Make stdout non-blocking
            fd = decoder_proc.stdout.fileno()
            flags = fcntl.fcntl(fd, fcntl.F_GETFL)
            fcntl.fcntl(fd, fcntl.F_SETFL, flags | os.O_NONBLOCK)

            # Wait for decoder to start and process pre-existing tracepoints
            time.sleep(0.5)

            # Step 4: Write MORE tracepoints AFTER decoder has started
            for msg in post_messages:
                result = subprocess.run(
                    [str(clltk_path), "trace", "-b", buffer_name, msg],
                    env=os.environ.copy(),
                    capture_output=True,
                )
                self.assertEqual(
                    result.returncode,
                    0,
                    f"Failed to write post-start tracepoint: {result.stderr.decode()}",
                )
                time.sleep(0.05)

            # Wait for decoder to process the new tracepoints
            time.sleep(0.8)

            # Step 5: Collect output and verify ALL messages appear
            collected_output = b""
            start_time = time.time()
            max_wait = 4.0
            found_messages = set()

            while time.time() - start_time < max_wait:
                ready, _, _ = select.select([decoder_proc.stdout], [], [], 0.1)
                if ready:
                    try:
                        chunk = decoder_proc.stdout.read(8192)
                        if chunk:
                            collected_output += chunk
                            for msg in all_messages:
                                if msg.encode() in collected_output:
                                    found_messages.add(msg)
                    except BlockingIOError:
                        pass

                if len(found_messages) == len(all_messages):
                    break

            # Send SIGINT and collect any remaining flushed output
            decoder_proc.send_signal(signal.SIGINT)

            try:
                remaining, _ = decoder_proc.communicate(timeout=2)
                if remaining:
                    collected_output += remaining
            except subprocess.TimeoutExpired:
                decoder_proc.kill()
                decoder_proc.wait()

            stdout_text = collected_output.decode("utf-8", errors="replace")

            # Verify PRE-EXISTING messages appeared
            for msg in pre_messages:
                self.assertIn(
                    msg,
                    stdout_text,
                    f"Pre-existing message '{msg}' not found in decoder output.\n"
                    f"Output: {stdout_text[:500]}...",
                )

            # Verify POST-START messages appeared
            for msg in post_messages:
                self.assertIn(
                    msg,
                    stdout_text,
                    f"Post-start message '{msg}' not found in decoder output.\n"
                    f"Output: {stdout_text[:500]}...",
                )

        except Exception as e:
            if decoder_proc.poll() is None:
                decoder_proc.kill()
                decoder_proc.wait()
            raise e

    def test_concurrent_writer_and_decoder(self):
        """
        Test decoder receiving tracepoints while writer continuously writes.
        """
        buffer_name = "ConcurrentTestBuffer"
        num_messages = 20

        clltk("buffer", "--buffer", buffer_name, "--size", "16KB")

        clltk_path = get_clltk_path()
        cmd = []
        if not is_asan_build():
            cmd.extend(["stdbuf", "-oL"])
        cmd.extend(
            [
                str(clltk_path),
                "live",
                self.trace_path,
                "--order-delay",
                "20",
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

        messages_written = []
        writer_error = None

        def writer_thread():
            nonlocal writer_error
            try:
                for i in range(num_messages):
                    msg = f"concurrent_msg_{i:04d}"
                    messages_written.append(msg)
                    result = subprocess.run(
                        [str(clltk_path), "trace", "-b", buffer_name, msg],
                        env=os.environ.copy(),
                        capture_output=True,
                    )
                    if result.returncode != 0:
                        writer_error = f"Failed to write message {i}: {result.stderr}"
                        break
                    time.sleep(0.02)
            except Exception as e:
                writer_error = str(e)

        try:
            # Make decoder stdout non-blocking
            fd = decoder_proc.stdout.fileno()
            flags = fcntl.fcntl(fd, fcntl.F_GETFL)
            fcntl.fcntl(fd, fcntl.F_SETFL, flags | os.O_NONBLOCK)

            time.sleep(0.2)  # Let decoder start

            # Start writer thread
            writer = threading.Thread(target=writer_thread)
            writer.start()

            # Collect decoder output while writer is running
            collected_output = b""
            start_time = time.time()
            max_wait = 5.0

            while time.time() - start_time < max_wait:
                ready, _, _ = select.select([decoder_proc.stdout], [], [], 0.1)
                if ready:
                    try:
                        chunk = decoder_proc.stdout.read(8192)
                        if chunk:
                            collected_output += chunk
                    except BlockingIOError:
                        pass

                # Check if writer is done and we've waited enough
                if not writer.is_alive():
                    time.sleep(0.3)  # Give decoder time to flush
                    try:
                        while True:
                            chunk = decoder_proc.stdout.read(8192)
                            if not chunk:
                                break
                            collected_output += chunk
                    except BlockingIOError:
                        pass
                    break

            writer.join(timeout=2)

            # Check for writer errors
            self.assertIsNone(writer_error, f"Writer thread error: {writer_error}")

            # Verify output
            stdout_text = collected_output.decode("utf-8", errors="replace")

            found_count = sum(1 for msg in messages_written if msg in stdout_text)

            # Should see most messages (allow some slack for timing)
            self.assertGreaterEqual(
                found_count,
                num_messages - 2,
                f"Only {found_count}/{len(messages_written)} messages appeared in decoder.\n"
                f"Sample output: {stdout_text[:1000]}...",
            )

        finally:
            if decoder_proc.stdout:
                decoder_proc.stdout.close()
            if decoder_proc.stderr:
                decoder_proc.stderr.close()

            decoder_proc.send_signal(signal.SIGINT)
            try:
                decoder_proc.wait(timeout=2)
            except subprocess.TimeoutExpired:
                decoder_proc.kill()
                decoder_proc.wait()


if __name__ == "__main__":
    unittest.main()
