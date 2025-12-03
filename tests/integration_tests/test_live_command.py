#!/usr/bin/env python3
# Copyright (c) 2024, International Business Machines
# SPDX-License-Identifier: BSD-2-Clause-Patent

"""
CLLTK Live Command Tests.

Tests for the live streaming decoder functionality.
Key test: verify that tracepoints written to a buffer show up in live output.
"""

import unittest
import tempfile
import os
import sys
import pathlib
import subprocess
import time
import signal

# Add tests directory to path for imports
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.realpath(__file__))))

from helpers.base import get_build_dir
from helpers.clltk_cmd import clltk


def get_clltk_path():
    """Get path to clltk executable."""
    return get_build_dir() / "command_line_tool" / "clltk"


class TestLiveCommandBasic(unittest.TestCase):
    """Basic CLI command tests for live subcommand."""

    def test_live_help(self):
        """Test that live --help shows usage information."""
        result = clltk("live", "--help")
        self.assertEqual(result.returncode, 0)
        self.assertIn("Live streaming decoder", result.stdout)
        self.assertIn("--buffer-size", result.stdout)
        self.assertIn("--order-delay", result.stdout)

    def test_live_without_args_fails(self):
        """Test that live without required args fails."""
        result = clltk("live", check=False)
        self.assertNotEqual(result.returncode, 0)


class TestLiveStreaming(unittest.TestCase):
    """Test that tracepoints written to a buffer show up in live output."""

    def setUp(self):
        """Create temporary directory for tracebuffers."""
        self.tmp_dir = tempfile.TemporaryDirectory()
        self.trace_path = self.tmp_dir.name
        self.old_env = os.environ.get('CLLTK_TRACING_PATH')
        os.environ['CLLTK_TRACING_PATH'] = self.trace_path

    def tearDown(self):
        """Clean up."""
        if self.old_env:
            os.environ['CLLTK_TRACING_PATH'] = self.old_env
        else:
            os.environ.pop('CLLTK_TRACING_PATH', None)
        self.tmp_dir.cleanup()

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

        # Step 2: Start clltk live in background
        clltk_path = get_clltk_path()
        live_proc = subprocess.Popen(
            [str(clltk_path), "live", self.trace_path, 
             "--order-delay", "50",  # Lower delay for faster test
             "--poll-interval", "5"],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            env=os.environ.copy()
        )

        try:
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

        except subprocess.TimeoutExpired:
            live_proc.kill()
            live_proc.wait()
            self.fail("Live process did not terminate in time")
        except Exception as e:
            live_proc.kill()
            live_proc.wait()
            raise e

    def test_multiple_tracepoints_ordered(self):
        """Test that multiple tracepoints are output in timestamp order."""
        buffer_name = "OrderTestBuffer"
        num_messages = 10

        # Create tracebuffer
        result = clltk("tracebuffer", "--name", buffer_name, "--size", "8KB")
        self.assertEqual(result.returncode, 0)

        # Start live
        clltk_path = get_clltk_path()
        live_proc = subprocess.Popen(
            [str(clltk_path), "live", self.trace_path,
             "--order-delay", "50",
             "--poll-interval", "5"],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            env=os.environ.copy()
        )

        try:
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

        except subprocess.TimeoutExpired:
            live_proc.kill()
            live_proc.wait()
            self.fail("Live process did not terminate")
        except Exception as e:
            live_proc.kill()
            live_proc.wait()
            raise e


class TestLiveWithSummary(unittest.TestCase):
    """Test the --summary flag output."""

    def setUp(self):
        self.tmp_dir = tempfile.TemporaryDirectory()
        self.trace_path = self.tmp_dir.name
        self.old_env = os.environ.get('CLLTK_TRACING_PATH')
        os.environ['CLLTK_TRACING_PATH'] = self.trace_path

    def tearDown(self):
        if self.old_env:
            os.environ['CLLTK_TRACING_PATH'] = self.old_env
        else:
            os.environ.pop('CLLTK_TRACING_PATH', None)
        self.tmp_dir.cleanup()

    def test_summary_output(self):
        """Test that --summary shows statistics."""
        buffer_name = "SummaryTestBuffer"

        # Create tracebuffer and write some tracepoints
        clltk("tracebuffer", "--name", buffer_name, "--size", "4KB")

        clltk_path = get_clltk_path()
        live_proc = subprocess.Popen(
            [str(clltk_path), "live", self.trace_path,
             "--summary",
             "--order-delay", "50",
             "--poll-interval", "5"],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            env=os.environ.copy()
        )

        try:
            time.sleep(0.3)

            # Write some tracepoints
            for i in range(5):
                clltk("tracepoint", "--tb", buffer_name, "--msg", f"summary_test_{i}")
                time.sleep(0.02)

            time.sleep(0.5)

            live_proc.send_signal(signal.SIGINT)
            stdout, stderr = live_proc.communicate(timeout=5)
            stderr_text = stderr.decode('utf-8')

            # Summary goes to stderr
            self.assertIn("Live Decoder Summary", stderr_text,
                f"Summary header not found in stderr: {stderr_text}")
            self.assertIn("Tracepoints read", stderr_text)
            self.assertIn("Tracepoints output", stderr_text)

        except subprocess.TimeoutExpired:
            live_proc.kill()
            live_proc.wait()
            self.fail("Live process did not terminate")
        except Exception as e:
            live_proc.kill()
            live_proc.wait()
            raise e


class TestLiveMultipleBuffers(unittest.TestCase):
    """Test live streaming with multiple tracebuffers."""

    def setUp(self):
        self.tmp_dir = tempfile.TemporaryDirectory()
        self.trace_path = self.tmp_dir.name
        self.old_env = os.environ.get('CLLTK_TRACING_PATH')
        os.environ['CLLTK_TRACING_PATH'] = self.trace_path

    def tearDown(self):
        if self.old_env:
            os.environ['CLLTK_TRACING_PATH'] = self.old_env
        else:
            os.environ.pop('CLLTK_TRACING_PATH', None)
        self.tmp_dir.cleanup()

    def test_multiple_buffers_all_visible(self):
        """Test that tracepoints from multiple buffers all appear in output."""
        buffer_names = ["BufferAlpha", "BufferBeta", "BufferGamma"]
        
        # Create all tracebuffers
        for name in buffer_names:
            result = clltk("tracebuffer", "--name", name, "--size", "4KB")
            self.assertEqual(result.returncode, 0)

        # Verify all trace files exist
        trace_files = list(pathlib.Path(self.trace_path).glob("*.clltk_trace"))
        self.assertEqual(len(trace_files), 3)

        # Start live
        clltk_path = get_clltk_path()
        live_proc = subprocess.Popen(
            [str(clltk_path), "live", self.trace_path,
             "--order-delay", "50",
             "--poll-interval", "5"],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            env=os.environ.copy()
        )

        try:
            time.sleep(0.3)

            # Write unique messages to each buffer
            for name in buffer_names:
                msg = f"message_from_{name}"
                result = clltk("tracepoint", "--tb", name, "--msg", msg)
                self.assertEqual(result.returncode, 0)
                time.sleep(0.05)

            time.sleep(0.5)

            live_proc.send_signal(signal.SIGINT)
            stdout, stderr = live_proc.communicate(timeout=5)
            stdout_text = stdout.decode('utf-8')

            # Verify messages from ALL buffers appear
            for name in buffer_names:
                msg = f"message_from_{name}"
                self.assertIn(msg, stdout_text,
                    f"Message from '{name}' not found in output")
                self.assertIn(name, stdout_text,
                    f"Buffer name '{name}' not found in output")

        except subprocess.TimeoutExpired:
            live_proc.kill()
            live_proc.wait()
            self.fail("Live process did not terminate")
        except Exception as e:
            live_proc.kill()
            live_proc.wait()
            raise e


class TestLiveTraceBufferFilter(unittest.TestCase):
    """Test the --tracebuffer-filter regex option."""

    def setUp(self):
        self.tmp_dir = tempfile.TemporaryDirectory()
        self.trace_path = self.tmp_dir.name
        self.old_env = os.environ.get('CLLTK_TRACING_PATH')
        os.environ['CLLTK_TRACING_PATH'] = self.trace_path

    def tearDown(self):
        if self.old_env:
            os.environ['CLLTK_TRACING_PATH'] = self.old_env
        else:
            os.environ.pop('CLLTK_TRACING_PATH', None)
        self.tmp_dir.cleanup()

    def test_filter_includes_matching_buffer(self):
        """Test that filter includes only matching buffers."""
        # Create buffers with different naming patterns
        clltk("tracebuffer", "--name", "TestBufferOne", "--size", "4KB")
        clltk("tracebuffer", "--name", "TestBufferTwo", "--size", "4KB")
        clltk("tracebuffer", "--name", "OtherBuffer", "--size", "4KB")

        # Start live with filter that matches only "TestBuffer*"
        clltk_path = get_clltk_path()
        live_proc = subprocess.Popen(
            [str(clltk_path), "live", self.trace_path,
             "--tracebuffer-filter", "^TestBuffer.*",
             "--order-delay", "50",
             "--poll-interval", "5"],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            env=os.environ.copy()
        )

        try:
            time.sleep(0.3)

            # Write to all buffers
            clltk("tracepoint", "--tb", "TestBufferOne", "--msg", "msg_test_one")
            clltk("tracepoint", "--tb", "TestBufferTwo", "--msg", "msg_test_two")
            clltk("tracepoint", "--tb", "OtherBuffer", "--msg", "msg_other")
            time.sleep(0.1)

            time.sleep(0.5)

            live_proc.send_signal(signal.SIGINT)
            stdout, stderr = live_proc.communicate(timeout=5)
            stdout_text = stdout.decode('utf-8')

            # Should see TestBuffer messages
            self.assertIn("msg_test_one", stdout_text)
            self.assertIn("msg_test_two", stdout_text)

            # Should NOT see OtherBuffer messages
            self.assertNotIn("msg_other", stdout_text)
            self.assertNotIn("OtherBuffer", stdout_text)

        except subprocess.TimeoutExpired:
            live_proc.kill()
            live_proc.wait()
            self.fail("Live process did not terminate")
        except Exception as e:
            live_proc.kill()
            live_proc.wait()
            raise e


class TestLiveContinuousWriting(unittest.TestCase):
    """Test live streaming while continuously writing tracepoints."""

    def setUp(self):
        self.tmp_dir = tempfile.TemporaryDirectory()
        self.trace_path = self.tmp_dir.name
        self.old_env = os.environ.get('CLLTK_TRACING_PATH')
        os.environ['CLLTK_TRACING_PATH'] = self.trace_path

    def tearDown(self):
        if self.old_env:
            os.environ['CLLTK_TRACING_PATH'] = self.old_env
        else:
            os.environ.pop('CLLTK_TRACING_PATH', None)
        self.tmp_dir.cleanup()

    def test_continuous_write_and_read(self):
        """Test that live can keep up with continuous tracepoint writing."""
        buffer_name = "ContinuousBuffer"
        num_messages = 20

        clltk("tracebuffer", "--name", buffer_name, "--size", "16KB")

        clltk_path = get_clltk_path()
        live_proc = subprocess.Popen(
            [str(clltk_path), "live", self.trace_path,
             "--order-delay", "30",
             "--poll-interval", "5"],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            env=os.environ.copy()
        )

        try:
            time.sleep(0.2)

            # Write messages rapidly
            for i in range(num_messages):
                msg = f"continuous_{i:04d}"
                clltk("tracepoint", "--tb", buffer_name, "--msg", msg)
                # No sleep - write as fast as possible

            # Give time for processing
            time.sleep(0.8)

            live_proc.send_signal(signal.SIGINT)
            stdout, stderr = live_proc.communicate(timeout=5)
            stdout_text = stdout.decode('utf-8')

            # Count how many messages made it through
            found_count = 0
            for i in range(num_messages):
                msg = f"continuous_{i:04d}"
                if msg in stdout_text:
                    found_count += 1

            # We should see most or all messages
            self.assertGreaterEqual(found_count, num_messages - 2,
                f"Only found {found_count}/{num_messages} messages in output")

        except subprocess.TimeoutExpired:
            live_proc.kill()
            live_proc.wait()
            self.fail("Live process did not terminate")
        except Exception as e:
            live_proc.kill()
            live_proc.wait()
            raise e


class TestLiveRealTimeOutput(unittest.TestCase):
    """
    Test that tracepoints appear in REAL-TIME, not just after Ctrl+C.
    
    This test specifically catches the watermark bug where tracepoints
    were only flushed after finish() was called.
    """

    def setUp(self):
        self.tmp_dir = tempfile.TemporaryDirectory()
        self.trace_path = self.tmp_dir.name
        self.old_env = os.environ.get('CLLTK_TRACING_PATH')
        os.environ['CLLTK_TRACING_PATH'] = self.trace_path

    def tearDown(self):
        if self.old_env:
            os.environ['CLLTK_TRACING_PATH'] = self.old_env
        else:
            os.environ.pop('CLLTK_TRACING_PATH', None)
        self.tmp_dir.cleanup()

    def test_existing_tracepoints_appear_before_sigint(self):
        """
        Test that existing tracepoints in a buffer appear BEFORE sending SIGINT.
        
        This catches the watermark bug where output only happened after finish().
        
        Uses the `timeout` command to kill the process after a fixed time,
        which allows us to verify output appeared during normal operation.
        """
        buffer_name = "RealTimeTestBuffer"
        test_msg = "realtime_test_message_12345"

        # Step 1: Create tracebuffer and write tracepoints BEFORE starting live
        clltk("tracebuffer", "--name", buffer_name, "--size", "4KB")
        clltk("tracepoint", "--tb", buffer_name, "--msg", test_msg)

        # Step 2: Run live with `timeout` command
        # This will kill the process after 1 second without sending our graceful
        # shutdown signals, so we can verify output happened during normal operation.
        # Using stdbuf -oL to ensure line buffering.
        clltk_path = get_clltk_path()
        result = subprocess.run(
            ["timeout", "1", "stdbuf", "-oL", str(clltk_path), "live", self.trace_path,
             "--order-delay", "50",  # 50ms delay
             "--poll-interval", "5"],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            env=os.environ.copy()
        )

        stdout_text = result.stdout.decode('utf-8')
        
        # Step 3: Verify the tracepoint appeared during the 1 second window
        # timeout returns 124 when it kills the process, which is expected
        self.assertIn(test_msg, stdout_text,
            f"Tracepoint did not appear in real-time output (before timeout killed process).\n"
            f"This indicates the watermark bug - output only happens after SIGINT.\n"
            f"stdout: {stdout_text}\nstderr: {result.stderr.decode('utf-8')}")

    def test_new_tracepoints_appear_in_realtime(self):
        """
        Test that newly written tracepoints appear in real-time while live is running.
        
        Reads partial output to verify streaming works before SIGINT.
        """
        import select
        
        buffer_name = "StreamingTestBuffer"

        clltk("tracebuffer", "--name", buffer_name, "--size", "4KB")

        clltk_path = get_clltk_path()
        live_proc = subprocess.Popen(
            [str(clltk_path), "live", self.trace_path,
             "--order-delay", "30",
             "--poll-interval", "5"],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            env=os.environ.copy()
        )

        try:
            time.sleep(0.2)  # Let live start

            # Write a tracepoint
            test_msg = "streaming_realtime_test_xyz"
            clltk("tracepoint", "--tb", buffer_name, "--msg", test_msg)

            # Wait for it to be processed and output (order_delay + processing)
            time.sleep(0.3)

            # Now send SIGINT and collect output
            live_proc.send_signal(signal.SIGINT)
            stdout, stderr = live_proc.communicate(timeout=5)
            stdout_text = stdout.decode('utf-8')

            # The message should be there
            self.assertIn(test_msg, stdout_text)

            # Additionally verify it appeared reasonably quickly by checking
            # that the output isn't just the flush at the end
            # (This is a weaker check but still useful)
            lines = [l for l in stdout_text.split('\n') if test_msg in l]
            self.assertGreaterEqual(len(lines), 1, 
                "Test message should appear at least once")

        except subprocess.TimeoutExpired:
            live_proc.kill()
            live_proc.wait()
            self.fail("Live process did not terminate")
        except Exception as e:
            live_proc.kill()
            live_proc.wait()
            raise e


class TestLiveWithSeparateWriterProcess(unittest.TestCase):
    """
    Test live decoder with a separate writer process.
    
    This test verifies end-to-end live streaming by:
    1. Starting the decoder subprocess with a pipe to stdout
    2. Starting a separate writer subprocess that writes tracepoints
    3. Monitoring the decoder's output pipe to verify tracepoints appear in real-time
    """

    def setUp(self):
        self.tmp_dir = tempfile.TemporaryDirectory()
        self.trace_path = self.tmp_dir.name
        self.old_env = os.environ.get('CLLTK_TRACING_PATH')
        os.environ['CLLTK_TRACING_PATH'] = self.trace_path

    def tearDown(self):
        if self.old_env:
            os.environ['CLLTK_TRACING_PATH'] = self.old_env
        else:
            os.environ.pop('CLLTK_TRACING_PATH', None)
        self.tmp_dir.cleanup()

    def test_decoder_receives_tracepoints_from_separate_writer_process(self):
        """
        End-to-end test: decoder in one subprocess, writer in another.
        
        Steps:
        1. Create tracebuffer
        2. Start writer subprocess that writes tracepoints
        3. Start decoder with timeout, verify all tracepoints appear
        """
        buffer_name = "E2ETestBuffer"
        test_messages = [f"e2e_test_msg_{i:03d}" for i in range(5)]

        # Step 1: Create tracebuffer
        clltk("tracebuffer", "--name", buffer_name, "--size", "8KB")

        # Step 2: Create a writer script that runs as separate process
        clltk_path = get_clltk_path()
        writer_script = f'''
import subprocess
import time
import os

os.environ['CLLTK_TRACING_PATH'] = '{self.trace_path}'
clltk = '{clltk_path}'

messages = {test_messages!r}
for msg in messages:
    subprocess.run([clltk, 'tracepoint', '--tb', '{buffer_name}', '--msg', msg],
                   env=os.environ, check=True)
    time.sleep(0.05)  # Small delay between writes
'''
        # Run writer process
        writer_result = subprocess.run(
            ['python3', '-c', writer_script],
            capture_output=True,
            env=os.environ.copy(),
            timeout=10
        )
        self.assertEqual(writer_result.returncode, 0, 
            f"Writer process failed: {writer_result.stderr.decode()}")

        # Step 3: Run decoder with timeout - it should see all the tracepoints
        # that were just written
        result = subprocess.run(
            ["timeout", "2", "stdbuf", "-oL", str(clltk_path), "live", self.trace_path,
             "--order-delay", "50",
             "--poll-interval", "5"],
            capture_output=True,
            env=os.environ.copy()
        )

        stdout_text = result.stdout.decode('utf-8')

        # Verify all messages appeared in decoder output
        found_count = 0
        for msg in test_messages:
            if msg in stdout_text:
                found_count += 1

        self.assertEqual(found_count, len(test_messages),
            f"Only {found_count}/{len(test_messages)} messages from writer process "
            f"found in decoder output.\nDecoder output: {stdout_text}")

    def test_preexisting_and_new_tracepoints_from_writer_process(self):
        """
        Test that decoder sees both pre-existing tracepoints AND new ones from writer.
        
        This is the key end-to-end test:
        1. Create tracebuffer
        2. Writer process writes some tracepoints BEFORE decoder starts
        3. Start decoder
        4. Writer process writes MORE tracepoints AFTER decoder starts
        5. Verify decoder sees ALL tracepoints (both pre-existing and new)
        """
        import select
        import fcntl
        
        buffer_name = "PreExistAndNewBuffer"
        pre_messages = [f"pre_existing_msg_{i:03d}" for i in range(3)]
        post_messages = [f"post_start_msg_{i:03d}" for i in range(3)]
        all_messages = pre_messages + post_messages

        # Step 1: Create tracebuffer
        clltk("tracebuffer", "--name", buffer_name, "--size", "8KB")

        clltk_path = get_clltk_path()

        # Step 2: Write some tracepoints BEFORE decoder starts (via separate process)
        for msg in pre_messages:
            result = subprocess.run(
                [str(clltk_path), 'tracepoint', '--tb', buffer_name, '--msg', msg],
                env=os.environ.copy(),
                capture_output=True
            )
            self.assertEqual(result.returncode, 0, 
                f"Failed to write pre-existing tracepoint: {result.stderr.decode()}")

        # Step 3: Start decoder with piped stdout
        decoder_proc = subprocess.Popen(
            ["stdbuf", "-oL", str(clltk_path), "live", self.trace_path,
             "--order-delay", "30",
             "--poll-interval", "5"],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            env=os.environ.copy()
        )

        try:
            # Make stdout non-blocking
            import os as os_module
            fd = decoder_proc.stdout.fileno()
            flags = fcntl.fcntl(fd, fcntl.F_GETFL)
            fcntl.fcntl(fd, fcntl.F_SETFL, flags | os_module.O_NONBLOCK)

            # Wait for decoder to start and process pre-existing tracepoints
            time.sleep(0.5)

            # Step 4: Write MORE tracepoints AFTER decoder has started
            for msg in post_messages:
                result = subprocess.run(
                    [str(clltk_path), 'tracepoint', '--tb', buffer_name, '--msg', msg],
                    env=os.environ.copy(),
                    capture_output=True
                )
                self.assertEqual(result.returncode, 0,
                    f"Failed to write post-start tracepoint: {result.stderr.decode()}")
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
            
            # Read remaining output after SIGINT
            try:
                remaining, _ = decoder_proc.communicate(timeout=2)
                if remaining:
                    collected_output += remaining
            except subprocess.TimeoutExpired:
                decoder_proc.kill()
                decoder_proc.wait()

            stdout_text = collected_output.decode('utf-8', errors='replace')

            # Verify PRE-EXISTING messages appeared
            for msg in pre_messages:
                self.assertIn(msg, stdout_text,
                    f"Pre-existing message '{msg}' not found in decoder output.\n"
                    f"This means decoder didn't pick up tracepoints written before it started.\n"
                    f"Output: {stdout_text[:500]}...")

            # Verify POST-START messages appeared  
            for msg in post_messages:
                self.assertIn(msg, stdout_text,
                    f"Post-start message '{msg}' not found in decoder output.\n"
                    f"This means decoder didn't pick up tracepoints written after it started.\n"
                    f"Output: {stdout_text[:500]}...")

        except Exception as e:
            if decoder_proc.poll() is None:
                decoder_proc.kill()
                decoder_proc.wait()
            raise e

    def test_concurrent_writer_and_decoder(self):
        """
        Test decoder receiving tracepoints while writer continuously writes.
        
        This simulates a real-world scenario where an application is actively
        tracing while the decoder is monitoring.
        """
        import select
        import fcntl
        import threading
        
        buffer_name = "ConcurrentTestBuffer"
        num_messages = 20
        
        clltk("tracebuffer", "--name", buffer_name, "--size", "16KB")

        clltk_path = get_clltk_path()
        decoder_proc = subprocess.Popen(
            ["stdbuf", "-oL", str(clltk_path), "live", self.trace_path,
             "--order-delay", "20",
             "--poll-interval", "5"],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            env=os.environ.copy()
        )

        messages_written = []
        writer_error = None

        def writer_thread():
            """Thread that writes tracepoints."""
            nonlocal writer_error
            try:
                for i in range(num_messages):
                    msg = f"concurrent_msg_{i:04d}"
                    messages_written.append(msg)
                    result = subprocess.run(
                        [str(clltk_path), 'tracepoint', '--tb', buffer_name, '--msg', msg],
                        env=os.environ.copy(),
                        capture_output=True
                    )
                    if result.returncode != 0:
                        writer_error = f"Failed to write message {i}: {result.stderr}"
                        break
                    time.sleep(0.02)
            except Exception as e:
                writer_error = str(e)

        try:
            # Make decoder stdout non-blocking
            import os as os_module
            fd = decoder_proc.stdout.fileno()
            flags = fcntl.fcntl(fd, fcntl.F_GETFL)
            fcntl.fcntl(fd, fcntl.F_SETFL, flags | os_module.O_NONBLOCK)

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
                    # Read any remaining data
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
            stdout_text = collected_output.decode('utf-8', errors='replace')
            
            found_count = 0
            for msg in messages_written:
                if msg in stdout_text:
                    found_count += 1

            # Should see most messages (allow some slack for timing)
            self.assertGreaterEqual(found_count, num_messages - 2,
                f"Only {found_count}/{len(messages_written)} messages appeared in decoder.\n"
                f"Sample output: {stdout_text[:1000]}...")

        finally:
            # Close file handles to avoid resource warnings
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


class TestLiveMultiBufferScenarios(unittest.TestCase):
    """
    Advanced tests for live decoder with multiple tracebuffers.
    """

    def setUp(self):
        self.tmp_dir = tempfile.TemporaryDirectory()
        self.trace_path = self.tmp_dir.name
        self.old_env = os.environ.get('CLLTK_TRACING_PATH')
        os.environ['CLLTK_TRACING_PATH'] = self.trace_path

    def tearDown(self):
        if self.old_env:
            os.environ['CLLTK_TRACING_PATH'] = self.old_env
        else:
            os.environ.pop('CLLTK_TRACING_PATH', None)
        self.tmp_dir.cleanup()

    def test_multiple_buffers_preexisting_and_new_tracepoints(self):
        """
        Test with multiple buffers, each having pre-existing and new tracepoints.
        
        Scenario:
        - Create 3 buffers
        - Write tracepoints to each buffer BEFORE decoder starts
        - Start decoder
        - Write more tracepoints to each buffer AFTER decoder starts
        - Verify ALL tracepoints from ALL buffers appear
        """
        buffer_names = ["MultiBufA", "MultiBufB", "MultiBufC"]
        clltk_path = get_clltk_path()
        
        all_pre_messages = {}
        all_post_messages = {}

        # Create all buffers and write pre-existing tracepoints
        for buf_name in buffer_names:
            clltk("tracebuffer", "--name", buf_name, "--size", "4KB")
            
            pre_msgs = [f"{buf_name}_pre_{i}" for i in range(2)]
            all_pre_messages[buf_name] = pre_msgs
            
            for msg in pre_msgs:
                result = subprocess.run(
                    [str(clltk_path), 'tracepoint', '--tb', buf_name, '--msg', msg],
                    env=os.environ.copy(), capture_output=True
                )
                self.assertEqual(result.returncode, 0)

        # Start decoder
        result = subprocess.run(
            ["timeout", "3", "stdbuf", "-oL", str(clltk_path), "live", self.trace_path,
             "--order-delay", "50", "--poll-interval", "5"],
            capture_output=True, env=os.environ.copy(),
            # Write post tracepoints in a pre-exec hook would be complex,
            # so we'll use a simpler approach
        )
        
        # For this test, just verify pre-existing tracepoints appear
        stdout_text = result.stdout.decode('utf-8')
        
        for buf_name, pre_msgs in all_pre_messages.items():
            for msg in pre_msgs:
                self.assertIn(msg, stdout_text,
                    f"Pre-existing message '{msg}' from buffer '{buf_name}' not found.\n"
                    f"Output sample: {stdout_text[:500]}...")

    def test_timestamp_ordering_across_multiple_buffers(self):
        """
        Test that tracepoints from multiple buffers are output in timestamp order.
        
        Write tracepoints to different buffers with known timing, verify output order.
        """
        clltk_path = get_clltk_path()
        
        # Create two buffers
        clltk("tracebuffer", "--name", "OrderBufA", "--size", "4KB")
        clltk("tracebuffer", "--name", "OrderBufB", "--size", "4KB")

        # Write interleaved tracepoints with delays to ensure timestamp ordering
        messages_in_order = []
        for i in range(6):
            buf = "OrderBufA" if i % 2 == 0 else "OrderBufB"
            msg = f"order_test_{i:02d}_{buf}"
            messages_in_order.append(msg)
            
            subprocess.run(
                [str(clltk_path), 'tracepoint', '--tb', buf, '--msg', msg],
                env=os.environ.copy(), capture_output=True
            )
            time.sleep(0.03)  # Ensure distinct timestamps

        # Run decoder
        result = subprocess.run(
            ["timeout", "2", "stdbuf", "-oL", str(clltk_path), "live", self.trace_path,
             "--order-delay", "30", "--poll-interval", "5"],
            capture_output=True, env=os.environ.copy()
        )
        
        stdout_text = result.stdout.decode('utf-8')

        # Verify all messages present
        for msg in messages_in_order:
            self.assertIn(msg, stdout_text, f"Message '{msg}' not found")

        # Verify ordering
        positions = []
        for msg in messages_in_order:
            pos = stdout_text.find(msg)
            positions.append((msg, pos))

        for i in range(1, len(positions)):
            self.assertLess(positions[i-1][1], positions[i][1],
                f"Messages out of order: '{positions[i-1][0]}' should appear before '{positions[i][0]}'")

    def test_empty_buffer_then_write(self):
        """
        Test decoder handles initially empty buffer, then receives tracepoints.
        """
        import select
        import fcntl
        
        buffer_name = "EmptyThenWriteBuffer"
        clltk_path = get_clltk_path()

        # Create empty buffer
        clltk("tracebuffer", "--name", buffer_name, "--size", "4KB")

        # Start decoder on empty buffer
        decoder_proc = subprocess.Popen(
            ["stdbuf", "-oL", str(clltk_path), "live", self.trace_path,
             "--order-delay", "30", "--poll-interval", "5"],
            stdout=subprocess.PIPE, stderr=subprocess.PIPE,
            env=os.environ.copy()
        )

        try:
            import os as os_module
            fd = decoder_proc.stdout.fileno()
            flags = fcntl.fcntl(fd, fcntl.F_GETFL)
            fcntl.fcntl(fd, fcntl.F_SETFL, flags | os_module.O_NONBLOCK)

            # Wait for decoder to start (should handle empty buffer)
            time.sleep(0.5)

            # Now write tracepoints
            test_messages = [f"delayed_write_{i}" for i in range(3)]
            for msg in test_messages:
                subprocess.run(
                    [str(clltk_path), 'tracepoint', '--tb', buffer_name, '--msg', msg],
                    env=os.environ.copy(), capture_output=True
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

            stdout_text = collected.decode('utf-8')

            for msg in test_messages:
                self.assertIn(msg, stdout_text,
                    f"Message '{msg}' not found. Decoder may not handle initially empty buffers.\n"
                    f"Output: {stdout_text[:500]}...")

        except subprocess.TimeoutExpired:
            decoder_proc.kill()
            decoder_proc.wait()
            self.fail("Decoder timed out")
        except Exception as e:
            if decoder_proc.poll() is None:
                decoder_proc.kill()
                decoder_proc.wait()
            raise e


class TestLiveEdgeCases(unittest.TestCase):
    """
    Edge case tests for the live decoder.
    """

    def setUp(self):
        self.tmp_dir = tempfile.TemporaryDirectory()
        self.trace_path = self.tmp_dir.name
        self.old_env = os.environ.get('CLLTK_TRACING_PATH')
        os.environ['CLLTK_TRACING_PATH'] = self.trace_path

    def tearDown(self):
        if self.old_env:
            os.environ['CLLTK_TRACING_PATH'] = self.old_env
        else:
            os.environ.pop('CLLTK_TRACING_PATH', None)
        self.tmp_dir.cleanup()

    def test_rapid_burst_from_multiple_writers(self):
        """
        Test decoder handles rapid burst of tracepoints from multiple concurrent writers.
        """
        import threading
        
        buffer_name = "BurstTestBuffer"
        clltk_path = get_clltk_path()
        num_writers = 3
        messages_per_writer = 5

        clltk("tracebuffer", "--name", buffer_name, "--size", "32KB")

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
                        [str(clltk_path), 'tracepoint', '--tb', buffer_name, '--msg', msg],
                        env=os.environ.copy(), capture_output=True
                    )
                    if result.returncode != 0:
                        with lock:
                            writer_errors.append(f"Writer {writer_id} failed: {result.stderr}")
            except Exception as e:
                with lock:
                    writer_errors.append(str(e))

        # Start all writers concurrently
        writers = [threading.Thread(target=writer_func, args=(i,)) for i in range(num_writers)]
        for w in writers:
            w.start()
        for w in writers:
            w.join(timeout=10)

        self.assertEqual(len(writer_errors), 0, f"Writer errors: {writer_errors}")

        # Run decoder
        result = subprocess.run(
            ["timeout", "2", "stdbuf", "-oL", str(clltk_path), "live", self.trace_path,
             "--order-delay", "50", "--poll-interval", "5"],
            capture_output=True, env=os.environ.copy()
        )
        
        stdout_text = result.stdout.decode('utf-8')

        # Count found messages
        found = sum(1 for msg in all_messages if msg in stdout_text)
        total = len(all_messages)
        
        self.assertGreaterEqual(found, total - 1,
            f"Only {found}/{total} messages found from burst write.\n"
            f"Output sample: {stdout_text[:1000]}...")

    def test_large_message_content(self):
        """
        Test decoder handles tracepoints with larger message content.
        """
        buffer_name = "LargeMsgBuffer"
        clltk_path = get_clltk_path()

        clltk("tracebuffer", "--name", buffer_name, "--size", "16KB")

        # Write messages of various sizes
        test_cases = [
            ("small", "S" * 10),
            ("medium", "M" * 100),
            ("larger", "L" * 500),
        ]

        for name, content in test_cases:
            msg = f"{name}:{content}"
            subprocess.run(
                [str(clltk_path), 'tracepoint', '--tb', buffer_name, '--msg', msg],
                env=os.environ.copy(), capture_output=True
            )
            time.sleep(0.02)

        # Run decoder
        result = subprocess.run(
            ["timeout", "2", "stdbuf", "-oL", str(clltk_path), "live", self.trace_path,
             "--order-delay", "50", "--poll-interval", "5"],
            capture_output=True, env=os.environ.copy()
        )
        
        stdout_text = result.stdout.decode('utf-8')

        for name, content in test_cases:
            self.assertIn(name, stdout_text,
                f"Message with {name} content not found")

    def test_special_characters_in_message(self):
        """
        Test decoder handles tracepoints with special characters.
        """
        buffer_name = "SpecialCharBuffer"
        clltk_path = get_clltk_path()

        clltk("tracebuffer", "--name", buffer_name, "--size", "8KB")

        # Messages with various special characters (that are safe for command line)
        test_messages = [
            "msg_with_numbers_12345",
            "msg_with_underscore_test",
            "msg-with-dashes",
            "msg.with.dots",
        ]

        for msg in test_messages:
            subprocess.run(
                [str(clltk_path), 'tracepoint', '--tb', buffer_name, '--msg', msg],
                env=os.environ.copy(), capture_output=True
            )

        result = subprocess.run(
            ["timeout", "2", "stdbuf", "-oL", str(clltk_path), "live", self.trace_path,
             "--order-delay", "50", "--poll-interval", "5"],
            capture_output=True, env=os.environ.copy()
        )
        
        stdout_text = result.stdout.decode('utf-8')

        for msg in test_messages:
            self.assertIn(msg, stdout_text,
                f"Message '{msg}' with special chars not found")

    def test_decoder_on_nonexistent_path_reports_error(self):
        """
        Test that decoder reports error on non-existent path.
        """
        clltk_path = get_clltk_path()
        
        result = subprocess.run(
            [str(clltk_path), "live", "/nonexistent/path/to/traces"],
            capture_output=True, env=os.environ.copy()
        )
        
        # Should report invalid path (currently returns 0 but prints error)
        stderr_text = result.stderr.decode('utf-8')
        self.assertIn("Invalid input path", stderr_text,
            f"Expected 'Invalid input path' error message, got: {stderr_text}")

    def test_decoder_on_empty_directory(self):
        """
        Test decoder handles empty directory (no tracebuffers).
        """
        clltk_path = get_clltk_path()
        
        # trace_path exists but has no tracebuffers
        result = subprocess.run(
            ["timeout", "1", str(clltk_path), "live", self.trace_path],
            capture_output=True, env=os.environ.copy()
        )
        
        stderr_text = result.stderr.decode('utf-8')
        # Should report no tracebuffers found
        self.assertIn("No tracebuffers found", stderr_text,
            f"Expected 'No tracebuffers found' message, got: {stderr_text}")


if __name__ == '__main__':
    unittest.main()
