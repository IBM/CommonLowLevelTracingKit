#!/usr/bin/env python3
# Copyright (c) 2024, International Business Machines
# SPDX-License-Identifier: BSD-2-Clause-Patent

"""
CLLTK Live Command Stress Tests.

Extreme stress tests for the live decoder.
These tests push the decoder to its limits with high-volume,
high-frequency tracepoint generation to verify stability and
proper handling of buffer overflow conditions.
"""

import os
import re
import signal
import subprocess
import sys
import threading
import time
import unittest

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.realpath(__file__))))
sys.path.insert(0, os.path.dirname(os.path.realpath(__file__)))

from helpers.clltk_cmd import clltk
from test_live_base import LiveTestCase, get_clltk_path, run_live_with_timeout


class TestLiveExtremeStress(LiveTestCase):
    """
    Extreme stress tests for the live decoder.
    """

    def test_high_volume_tracepoints_with_summary(self):
        """
        Test decoder under high volume load and verify summary statistics.
        """
        buffer_name = "HighVolumeBuffer"
        clltk_path = get_clltk_path()
        num_messages = 500

        # Create a larger buffer to hold more tracepoints
        clltk("tracebuffer", "--name", buffer_name, "--size", "256KB")

        # Write many tracepoints as fast as possible
        for i in range(num_messages):
            subprocess.run(
                [str(clltk_path), 'tracepoint', '--tb', buffer_name, '--msg', f"highvol_{i:05d}"],
                env=os.environ.copy(), capture_output=True
            )

        # Run decoder with summary enabled and small buffer to force drops
        result = subprocess.run(
            ["timeout", "3", str(clltk_path), "live", self.trace_path,
             "--summary", "--buffer-size", "100", "--order-delay", "10", "--poll-interval", "1"],
            capture_output=True, env=os.environ.copy()
        )

        stderr_text = result.stderr.decode('utf-8')

        # Verify summary is present
        self.assertIn("Live Decoder Summary", stderr_text,
            f"Summary not found in stderr: {stderr_text}")
        self.assertIn("Tracepoints read:", stderr_text)
        self.assertIn("Tracepoints output:", stderr_text)
        self.assertIn("Tracepoints dropped:", stderr_text)
        self.assertIn("Buffer high water:", stderr_text)

        # Parse summary values
        read_match = re.search(r'Tracepoints read:\s+(\d+)', stderr_text)
        output_match = re.search(r'Tracepoints output:\s+(\d+)', stderr_text)
        dropped_match = re.search(r'Tracepoints dropped:\s+(\d+)', stderr_text)
        high_water_match = re.search(r'Buffer high water:\s+(\d+)', stderr_text)

        self.assertIsNotNone(read_match, "Could not parse 'Tracepoints read'")
        self.assertIsNotNone(output_match, "Could not parse 'Tracepoints output'")
        self.assertIsNotNone(dropped_match, "Could not parse 'Tracepoints dropped'")
        self.assertIsNotNone(high_water_match, "Could not parse 'Buffer high water'")

        read_count = int(read_match.group(1))
        output_count = int(output_match.group(1))
        dropped_count = int(dropped_match.group(1))
        high_water = int(high_water_match.group(1))

        # Verify invariants
        self.assertEqual(read_count, output_count + dropped_count,
            f"read ({read_count}) should equal output ({output_count}) + dropped ({dropped_count})")
        
        # High water should be at most buffer size (100) 
        self.assertLessEqual(high_water, 100,
            f"High water ({high_water}) should not exceed buffer size (100)")
        
        # We should have read all the messages we wrote
        self.assertEqual(read_count, num_messages,
            f"Expected to read {num_messages} tracepoints, got {read_count}")

    def test_buffer_overflow_with_small_buffer(self):
        """
        Test that decoder handles buffer overflow gracefully.
        """
        buffer_name = "OverflowTestBuffer"
        clltk_path = get_clltk_path()
        num_messages = 200

        clltk("tracebuffer", "--name", buffer_name, "--size", "64KB")

        # Write messages
        for i in range(num_messages):
            subprocess.run(
                [str(clltk_path), 'tracepoint', '--tb', buffer_name, '--msg', f"overflow_{i:04d}"],
                env=os.environ.copy(), capture_output=True
            )

        # Run with very small buffer to force drops
        result = subprocess.run(
            ["timeout", "2", str(clltk_path), "live", self.trace_path,
             "--summary", "--buffer-size", "10", "--order-delay", "5", "--poll-interval", "1"],
            capture_output=True, env=os.environ.copy()
        )

        stderr_text = result.stderr.decode('utf-8')

        # Verify we got drops
        dropped_match = re.search(r'Tracepoints dropped:\s+(\d+)', stderr_text)
        self.assertIsNotNone(dropped_match, f"Could not parse dropped count: {stderr_text}")
        
        dropped_count = int(dropped_match.group(1))
        self.assertGreater(dropped_count, 0,
            f"Expected some drops with buffer size 10 and {num_messages} messages")

    def test_multiple_buffers_high_volume(self):
        """
        Test decoder with multiple tracebuffers under high volume.
        """
        clltk_path = get_clltk_path()
        num_buffers = 5
        messages_per_buffer = 100
        buffer_names = [f"MultiBuf_{i}" for i in range(num_buffers)]

        # Create all buffers
        for name in buffer_names:
            clltk("tracebuffer", "--name", name, "--size", "64KB")

        all_messages = []
        errors = []
        lock = threading.Lock()

        def write_to_buffer(buf_name, buf_idx):
            try:
                for i in range(messages_per_buffer):
                    msg = f"multi_{buf_idx}_{i:04d}"
                    with lock:
                        all_messages.append(msg)
                    result = subprocess.run(
                        [str(clltk_path), 'tracepoint', '--tb', buf_name, '--msg', msg],
                        env=os.environ.copy(), capture_output=True
                    )
                    if result.returncode != 0:
                        with lock:
                            errors.append(f"Failed to write to {buf_name}: {result.stderr}")
            except Exception as e:
                with lock:
                    errors.append(str(e))

        # Write to all buffers concurrently
        writers = [
            threading.Thread(target=write_to_buffer, args=(name, idx))
            for idx, name in enumerate(buffer_names)
        ]
        for w in writers:
            w.start()
        for w in writers:
            w.join(timeout=30)

        self.assertEqual(len(errors), 0, f"Write errors: {errors}")

        # Run decoder with summary
        result = subprocess.run(
            ["timeout", "5", str(clltk_path), "live", self.trace_path,
             "--summary", "--buffer-size", "1000", "--order-delay", "20", "--poll-interval", "2"],
            capture_output=True, env=os.environ.copy()
        )

        stdout_text = result.stdout.decode('utf-8')
        stderr_text = result.stderr.decode('utf-8')

        # Verify all buffer names appear in output
        for name in buffer_names:
            self.assertIn(name, stdout_text,
                f"Buffer '{name}' not found in output")

        # Parse summary
        read_match = re.search(r'Tracepoints read:\s+(\d+)', stderr_text)
        self.assertIsNotNone(read_match, f"Could not parse read count: {stderr_text}")
        
        read_count = int(read_match.group(1))
        expected_total = num_buffers * messages_per_buffer
        self.assertEqual(read_count, expected_total,
            f"Expected {expected_total} tracepoints, got {read_count}")

    def test_sustained_high_frequency_writes(self):
        """
        Test decoder during sustained high-frequency writes.
        """
        buffer_name = "SustainedBuffer"
        clltk_path = get_clltk_path()
        write_duration_seconds = 3
        
        clltk("tracebuffer", "--name", buffer_name, "--size", "512KB")

        # Start decoder
        decoder_proc = subprocess.Popen(
            ["stdbuf", "-oL", str(clltk_path), "live", self.trace_path,
             "--summary", "--buffer-size", "5000", "--order-delay", "15", "--poll-interval", "2"],
            stdout=subprocess.PIPE, stderr=subprocess.PIPE,
            env=os.environ.copy()
        )

        messages_written = []
        writer_done = threading.Event()
        writer_error = None

        def writer_thread():
            nonlocal writer_error
            try:
                start_time = time.time()
                msg_id = 0
                while time.time() - start_time < write_duration_seconds:
                    msg = f"sustained_{msg_id:06d}"
                    messages_written.append(msg)
                    result = subprocess.run(
                        [str(clltk_path), 'tracepoint', '--tb', buffer_name, '--msg', msg],
                        env=os.environ.copy(), capture_output=True
                    )
                    if result.returncode != 0:
                        writer_error = f"Write failed: {result.stderr.decode()}"
                        break
                    msg_id += 1
            except Exception as e:
                writer_error = str(e)
            finally:
                writer_done.set()

        try:
            time.sleep(0.3)

            writer = threading.Thread(target=writer_thread)
            writer.start()
            writer.join(timeout=write_duration_seconds + 5)
            
            self.assertIsNone(writer_error, f"Writer error: {writer_error}")

            time.sleep(1)

            decoder_proc.send_signal(signal.SIGINT)
            stdout, stderr = decoder_proc.communicate(timeout=5)

            stderr_text = stderr.decode('utf-8')

            read_match = re.search(r'Tracepoints read:\s+(\d+)', stderr_text)
            output_match = re.search(r'Tracepoints output:\s+(\d+)', stderr_text)
            dropped_match = re.search(r'Tracepoints dropped:\s+(\d+)', stderr_text)

            self.assertIsNotNone(read_match, f"Could not parse read: {stderr_text}")
            
            read_count = int(read_match.group(1))
            output_count = int(output_match.group(1)) if output_match else 0
            dropped_count = int(dropped_match.group(1)) if dropped_match else 0

            total_written = len(messages_written)
            
            self.assertEqual(read_count, total_written,
                f"Expected to read {total_written}, got {read_count}")
            
            self.assertEqual(read_count, output_count + dropped_count,
                f"read ({read_count}) != output ({output_count}) + dropped ({dropped_count})")

            print(f"\nSustained write test results:")
            print(f"  Duration: {write_duration_seconds}s")
            print(f"  Messages written: {total_written}")
            print(f"  Throughput: {total_written / write_duration_seconds:.1f} msg/s")
            print(f"  Output: {output_count}, Dropped: {dropped_count}")

        except subprocess.TimeoutExpired:
            decoder_proc.kill()
            decoder_proc.wait()
            self.fail("Decoder did not terminate")
        except Exception as e:
            if decoder_proc.poll() is None:
                decoder_proc.kill()
                decoder_proc.wait()
            raise e

    def test_unlimited_buffer_no_drops(self):
        """
        Test that unlimited buffer (size=0) prevents drops.
        """
        buffer_name = "UnlimitedBuffer"
        clltk_path = get_clltk_path()
        num_messages = 300

        clltk("tracebuffer", "--name", buffer_name, "--size", "128KB")

        for i in range(num_messages):
            subprocess.run(
                [str(clltk_path), 'tracepoint', '--tb', buffer_name, '--msg', f"unlimited_{i:04d}"],
                env=os.environ.copy(), capture_output=True
            )

        result = subprocess.run(
            ["timeout", "3", str(clltk_path), "live", self.trace_path,
             "--summary", "--buffer-size", "0", "--order-delay", "10", "--poll-interval", "1"],
            capture_output=True, env=os.environ.copy()
        )

        stderr_text = result.stderr.decode('utf-8')

        dropped_match = re.search(r'Tracepoints dropped:\s+(\d+)', stderr_text)
        self.assertIsNotNone(dropped_match, f"Could not parse dropped: {stderr_text}")
        
        dropped_count = int(dropped_match.group(1))
        self.assertEqual(dropped_count, 0,
            f"Expected 0 drops with unlimited buffer, got {dropped_count}")

    def test_interleaved_multi_buffer_ordering(self):
        """
        Test that tracepoints from multiple buffers maintain timestamp ordering.
        """
        clltk_path = get_clltk_path()
        num_buffers = 3
        rounds = 20
        buffer_names = [f"OrderBuf_{i}" for i in range(num_buffers)]

        for name in buffer_names:
            clltk("tracebuffer", "--name", name, "--size", "32KB")

        messages_in_order = []
        for round_num in range(rounds):
            for buf_idx, buf_name in enumerate(buffer_names):
                msg = f"order_r{round_num:02d}_b{buf_idx}"
                messages_in_order.append(msg)
                subprocess.run(
                    [str(clltk_path), 'tracepoint', '--tb', buf_name, '--msg', msg],
                    env=os.environ.copy(), capture_output=True
                )
                time.sleep(0.005)

        result = run_live_with_timeout(self.trace_path, timeout_seconds=3,
                                       order_delay=30, poll_interval=2)

        stdout_text = result.stdout.decode('utf-8')

        for msg in messages_in_order:
            self.assertIn(msg, stdout_text, f"Message '{msg}' not found")

        positions = []
        for msg in messages_in_order:
            pos = stdout_text.find(msg)
            if pos >= 0:
                positions.append((msg, pos))

        for i in range(1, len(positions)):
            self.assertLess(positions[i-1][1], positions[i][1],
                f"Out of order: '{positions[i-1][0]}' at {positions[i-1][1]} "
                f"should come before '{positions[i][0]}' at {positions[i][1]}")

    def test_rapid_start_stop_cycles(self):
        """
        Test decoder stability under rapid start/stop cycles.
        """
        buffer_name = "CycleBuffer"
        clltk_path = get_clltk_path()
        num_cycles = 10

        clltk("tracebuffer", "--name", buffer_name, "--size", "16KB")

        for i in range(20):
            subprocess.run(
                [str(clltk_path), 'tracepoint', '--tb', buffer_name, '--msg', f"cycle_init_{i}"],
                env=os.environ.copy(), capture_output=True
            )

        for cycle in range(num_cycles):
            cycle_msg = f"cycle_marker_{cycle:02d}"
            subprocess.run(
                [str(clltk_path), 'tracepoint', '--tb', buffer_name, '--msg', cycle_msg],
                env=os.environ.copy(), capture_output=True
            )

            result = subprocess.run(
                ["timeout", "0.5", str(clltk_path), "live", self.trace_path,
                 "--order-delay", "10", "--poll-interval", "2"],
                capture_output=True, env=os.environ.copy()
            )

            self.assertIn(result.returncode, [0, 124],
                f"Cycle {cycle}: Unexpected return code {result.returncode}")

    def test_maximum_message_throughput(self):
        """
        Measure maximum message throughput the decoder can handle.
        """
        buffer_name = "ThroughputBuffer"
        clltk_path = get_clltk_path()
        test_duration = 2

        clltk("tracebuffer", "--name", buffer_name, "--size", "1MB")

        decoder_proc = subprocess.Popen(
            [str(clltk_path), "live", self.trace_path,
             "--summary", "--buffer-size", "50000", "--order-delay", "5", "--poll-interval", "1"],
            stdout=subprocess.PIPE, stderr=subprocess.PIPE,
            env=os.environ.copy()
        )

        messages_written = 0
        writer_done = threading.Event()

        def writer_thread():
            nonlocal messages_written
            start_time = time.time()
            while time.time() - start_time < test_duration:
                result = subprocess.run(
                    [str(clltk_path), 'tracepoint', '--tb', buffer_name, 
                     '--msg', f"tp_{messages_written}"],
                    env=os.environ.copy(), capture_output=True
                )
                if result.returncode == 0:
                    messages_written += 1
            writer_done.set()

        try:
            time.sleep(0.2)

            writer = threading.Thread(target=writer_thread)
            writer.start()
            writer.join(timeout=test_duration + 5)

            time.sleep(1)

            decoder_proc.send_signal(signal.SIGINT)
            stdout, stderr = decoder_proc.communicate(timeout=5)
            stderr_text = stderr.decode('utf-8')

            read_match = re.search(r'Tracepoints read:\s+(\d+)', stderr_text)
            output_match = re.search(r'Tracepoints output:\s+(\d+)', stderr_text)
            dropped_match = re.search(r'Tracepoints dropped:\s+(\d+)', stderr_text)

            if read_match and output_match and dropped_match:
                read_count = int(read_match.group(1))
                output_count = int(output_match.group(1))
                dropped_count = int(dropped_match.group(1))

                print(f"\nThroughput test results:")
                print(f"  Duration: {test_duration}s")
                print(f"  Written: {messages_written}")
                print(f"  Read: {read_count}")
                print(f"  Output: {output_count}")
                print(f"  Dropped: {dropped_count}")
                print(f"  Write rate: {messages_written / test_duration:.1f} msg/s")
                print(f"  Output rate: {output_count / test_duration:.1f} msg/s")
                print(f"  Drop rate: {100 * dropped_count / max(1, read_count):.1f}%")

                self.assertEqual(read_count, messages_written,
                    f"Read count ({read_count}) should match written ({messages_written})")
                self.assertEqual(read_count, output_count + dropped_count,
                    "Accounting mismatch")

        except subprocess.TimeoutExpired:
            decoder_proc.kill()
            decoder_proc.wait()
            self.fail("Decoder did not terminate")
        except Exception as e:
            if decoder_proc.poll() is None:
                decoder_proc.kill()
                decoder_proc.wait()
            raise e

    def test_stress_with_varying_message_sizes(self):
        """
        Stress test with messages of varying sizes.
        """
        buffer_name = "VarSizeBuffer"
        clltk_path = get_clltk_path()
        num_messages = 200

        clltk("tracebuffer", "--name", buffer_name, "--size", "512KB")

        sizes = [10, 50, 100, 200, 500]
        for i in range(num_messages):
            size = sizes[i % len(sizes)]
            msg = f"sz{size}_" + "X" * (size - 10)
            subprocess.run(
                [str(clltk_path), 'tracepoint', '--tb', buffer_name, '--msg', msg],
                env=os.environ.copy(), capture_output=True
            )

        result = subprocess.run(
            ["timeout", "3", str(clltk_path), "live", self.trace_path,
             "--summary", "--buffer-size", "500", "--order-delay", "15", "--poll-interval", "2"],
            capture_output=True, env=os.environ.copy()
        )

        stderr_text = result.stderr.decode('utf-8')

        read_match = re.search(r'Tracepoints read:\s+(\d+)', stderr_text)
        self.assertIsNotNone(read_match, f"Could not parse summary: {stderr_text}")
        
        read_count = int(read_match.group(1))
        self.assertEqual(read_count, num_messages,
            f"Expected {num_messages} reads, got {read_count}")


if __name__ == '__main__':
    unittest.main()
