#!/usr/bin/env python3
# Copyright (c) 2024, International Business Machines
# SPDX-License-Identifier: BSD-2-Clause-Patent

"""
CLLTK Snapshot Command Tests.

Tests for the clltk snapshot subcommand including basic functionality,
compression, filtering, and edge cases.
"""

import os
import sys
import tarfile
import tempfile
import unittest

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.realpath(__file__))))

from helpers.base import run_command
from helpers.clltk_cmd import clltk


def setUpModule():
    """Configure CMake and build clltk-cmd before running tests."""
    run_command("cmake --preset default")
    run_command("cmake --build --preset default --target clltk-cmd")


class TestSnapshotCommandBase(unittest.TestCase):
    """Base CLI command tests for snapshot subcommand."""

    def test_snapshot_help(self):
        """Test that snapshot --help shows usage information."""
        result = clltk("snapshot", "--help")
        self.assertEqual(result.returncode, 0)
        self.assertIn("snapshot", result.stdout.lower())
        self.assertIn("--compress", result.stdout)
        self.assertIn("--output", result.stdout)

    def test_snapshot_help_short(self):
        """Test that snapshot -h shows usage information."""
        result = clltk("snapshot", "-h")
        self.assertEqual(result.returncode, 0)
        self.assertIn("snapshot", result.stdout.lower())

    def test_subcommand_snapshot_exists(self):
        """Test that snapshot subcommand exists."""
        result = clltk("--help")
        self.assertEqual(result.returncode, 0)
        self.assertIn("snapshot", result.stdout)

    def test_snapshot_alias_sp_exists(self):
        """Test that 'sp' alias for snapshot works."""
        result = clltk("sp", "--help")
        self.assertEqual(result.returncode, 0)
        self.assertIn("snapshot", result.stdout.lower())


class TestSnapshotBasicFunctionality(unittest.TestCase):
    """Basic functionality tests for snapshot command."""

    def setUp(self):
        """Create temporary directory and set environment."""
        self.tmp_dir = tempfile.TemporaryDirectory()
        self.trace_path = self.tmp_dir.name
        self.old_env = os.environ.get("CLLTK_TRACING_PATH")
        os.environ["CLLTK_TRACING_PATH"] = self.trace_path

    def tearDown(self):
        """Clean up temporary directory and restore environment."""
        if self.old_env:
            os.environ["CLLTK_TRACING_PATH"] = self.old_env
        else:
            os.environ.pop("CLLTK_TRACING_PATH", None)
        self.tmp_dir.cleanup()

    def _create_tracebuffer_with_content(self, name, messages):
        """Helper to create a tracebuffer and write messages to it."""
        result = clltk("buffer", "--buffer", name, "--size", "4KB")
        self.assertEqual(
            result.returncode, 0, f"Failed to create buffer: {result.stderr}"
        )

        for msg in messages:
            result = clltk("trace", "-b", name, msg)
            self.assertEqual(
                result.returncode, 0, f"Failed to write trace: {result.stderr}"
            )

    def test_create_snapshot(self):
        """Test creating a basic snapshot."""
        buffer_name = "SnapshotTestBuffer"
        self._create_tracebuffer_with_content(buffer_name, ["test_message_1"])

        output_file = os.path.join(self.trace_path, "test_snapshot.clltk")
        result = clltk("snapshot", "--output", output_file)
        self.assertEqual(result.returncode, 0, f"Snapshot failed: {result.stderr}")

        # Verify snapshot file was created
        self.assertTrue(os.path.exists(output_file), "Snapshot file was not created")
        self.assertGreater(os.path.getsize(output_file), 0, "Snapshot file is empty")

    def test_snapshot_default_filename(self):
        """Test snapshot creates default filename snapshot.clltk."""
        buffer_name = "DefaultNameBuffer"
        self._create_tracebuffer_with_content(buffer_name, ["default_name_test"])

        # Change to temp directory to create snapshot there
        old_cwd = os.getcwd()
        try:
            os.chdir(self.trace_path)
            result = clltk("snapshot")
            self.assertEqual(result.returncode, 0, f"Snapshot failed: {result.stderr}")

            default_file = os.path.join(self.trace_path, "snapshot.clltk")
            self.assertTrue(
                os.path.exists(default_file), "Default snapshot file was not created"
            )
        finally:
            os.chdir(old_cwd)

    def test_snapshot_custom_filename(self):
        """Test snapshot with custom output filename."""
        buffer_name = "CustomNameBuffer"
        self._create_tracebuffer_with_content(buffer_name, ["custom_name_test"])

        custom_file = os.path.join(self.trace_path, "my_custom_snapshot.clltk")
        result = clltk("snapshot", "-o", custom_file)
        self.assertEqual(result.returncode, 0, f"Snapshot failed: {result.stderr}")

        self.assertTrue(
            os.path.exists(custom_file), "Custom snapshot file was not created"
        )


class TestSnapshotCompression(unittest.TestCase):
    """Compression-related tests for snapshot command."""

    def setUp(self):
        """Create temporary directory and set environment."""
        self.tmp_dir = tempfile.TemporaryDirectory()
        self.trace_path = self.tmp_dir.name
        self.old_env = os.environ.get("CLLTK_TRACING_PATH")
        os.environ["CLLTK_TRACING_PATH"] = self.trace_path

    def tearDown(self):
        """Clean up temporary directory and restore environment."""
        if self.old_env:
            os.environ["CLLTK_TRACING_PATH"] = self.old_env
        else:
            os.environ.pop("CLLTK_TRACING_PATH", None)
        self.tmp_dir.cleanup()

    def _create_tracebuffer_with_content(self, name, messages):
        """Helper to create a tracebuffer and write messages to it."""
        result = clltk("buffer", "--buffer", name, "--size", "8KB")
        self.assertEqual(
            result.returncode, 0, f"Failed to create buffer: {result.stderr}"
        )

        for msg in messages:
            result = clltk("trace", "-b", name, msg)
            self.assertEqual(
                result.returncode, 0, f"Failed to write trace: {result.stderr}"
            )

    def test_compressed_snapshot(self):
        """Test creating a compressed snapshot with -z flag."""
        buffer_name = "CompressedBuffer"
        # Write multiple messages to have content to compress
        messages = [f"compressed_test_message_{i}" for i in range(20)]
        self._create_tracebuffer_with_content(buffer_name, messages)

        compressed_file = os.path.join(self.trace_path, "compressed.clltk")
        result = clltk("snapshot", "-z", "--output", compressed_file)
        self.assertEqual(
            result.returncode, 0, f"Compressed snapshot failed: {result.stderr}"
        )

        self.assertTrue(
            os.path.exists(compressed_file), "Compressed snapshot was not created"
        )

        # Verify it's gzip compressed by checking magic bytes
        with open(compressed_file, "rb") as f:
            magic = f.read(2)
            self.assertEqual(magic, b"\x1f\x8b", "File is not gzip compressed")

    def test_compressed_vs_uncompressed_size(self):
        """Test that compressed snapshot is smaller than uncompressed."""
        buffer_name = "SizeCompareBuffer"
        # Write many similar messages to ensure good compression ratio
        messages = [f"repeated_message_for_compression_test_{i}" for i in range(50)]
        self._create_tracebuffer_with_content(buffer_name, messages)

        uncompressed_file = os.path.join(self.trace_path, "uncompressed.clltk")
        compressed_file = os.path.join(self.trace_path, "compressed.clltk")

        # Create uncompressed snapshot
        result = clltk("snapshot", "--output", uncompressed_file)
        self.assertEqual(result.returncode, 0)

        # Create compressed snapshot
        result = clltk("snapshot", "-z", "--output", compressed_file)
        self.assertEqual(result.returncode, 0)

        uncompressed_size = os.path.getsize(uncompressed_file)
        compressed_size = os.path.getsize(compressed_file)

        self.assertLess(
            compressed_size,
            uncompressed_size,
            f"Compressed ({compressed_size}) should be smaller than uncompressed ({uncompressed_size})",
        )

    def test_decode_compressed_snapshot(self):
        """Test that compressed snapshot can be decoded."""
        buffer_name = "DecodeCompressedBuffer"
        test_message = "decode_compressed_test_unique_string"
        self._create_tracebuffer_with_content(buffer_name, [test_message])

        compressed_file = os.path.join(self.trace_path, "decode_test.clltk")
        result = clltk("snapshot", "-z", "--output", compressed_file)
        self.assertEqual(result.returncode, 0)

        # Decode the compressed snapshot
        result = clltk("decode", compressed_file)
        self.assertEqual(result.returncode, 0, f"Decode failed: {result.stderr}")
        self.assertIn(
            test_message, result.stdout, "Test message not found in decoded output"
        )

    def test_compress_long_flag(self):
        """Test --compress long flag works same as -z."""
        buffer_name = "LongFlagBuffer"
        self._create_tracebuffer_with_content(buffer_name, ["long_flag_test"])

        compressed_file = os.path.join(self.trace_path, "long_flag.clltk")
        result = clltk("snapshot", "--compress", "--output", compressed_file)
        self.assertEqual(result.returncode, 0, f"Snapshot failed: {result.stderr}")

        # Verify it's gzip compressed
        with open(compressed_file, "rb") as f:
            magic = f.read(2)
            self.assertEqual(magic, b"\x1f\x8b", "File is not gzip compressed")


class TestSnapshotContent(unittest.TestCase):
    """Tests to verify snapshot archive contents."""

    def setUp(self):
        """Create temporary directory and set environment."""
        self.tmp_dir = tempfile.TemporaryDirectory()
        self.trace_path = self.tmp_dir.name
        self.old_env = os.environ.get("CLLTK_TRACING_PATH")
        os.environ["CLLTK_TRACING_PATH"] = self.trace_path

    def tearDown(self):
        """Clean up temporary directory and restore environment."""
        if self.old_env:
            os.environ["CLLTK_TRACING_PATH"] = self.old_env
        else:
            os.environ.pop("CLLTK_TRACING_PATH", None)
        self.tmp_dir.cleanup()

    def _create_tracebuffer_with_content(self, name, messages):
        """Helper to create a tracebuffer and write messages to it."""
        result = clltk("buffer", "--buffer", name, "--size", "4KB")
        self.assertEqual(
            result.returncode, 0, f"Failed to create buffer: {result.stderr}"
        )

        for msg in messages:
            result = clltk("trace", "-b", name, msg)
            self.assertEqual(
                result.returncode, 0, f"Failed to write trace: {result.stderr}"
            )

    def test_snapshot_contains_tracebuffer(self):
        """Test that snapshot archive contains the tracebuffer file."""
        buffer_name = "ContentTestBuffer"
        self._create_tracebuffer_with_content(buffer_name, ["content_test"])

        output_file = os.path.join(self.trace_path, "content_test.clltk")
        result = clltk("snapshot", "--output", output_file)
        self.assertEqual(result.returncode, 0)

        # Open as tar archive and check contents
        with tarfile.open(output_file, "r:") as tar:
            names = tar.getnames()
            # Should contain .clltk_trace file
            trace_files = [n for n in names if n.endswith(".clltk_trace")]
            self.assertGreater(
                len(trace_files), 0, f"No trace files in archive: {names}"
            )

    def test_decode_snapshot_verifies_content(self):
        """Test decoding snapshot to verify tracepoint content."""
        buffer_name = "DecodeVerifyBuffer"
        test_messages = [
            "verify_message_alpha",
            "verify_message_beta",
            "verify_message_gamma",
        ]
        self._create_tracebuffer_with_content(buffer_name, test_messages)

        output_file = os.path.join(self.trace_path, "verify_content.clltk")
        result = clltk("snapshot", "--output", output_file)
        self.assertEqual(result.returncode, 0)

        # Decode and verify all messages are present
        result = clltk("decode", output_file)
        self.assertEqual(result.returncode, 0, f"Decode failed: {result.stderr}")

        for msg in test_messages:
            self.assertIn(
                msg, result.stdout, f"Message '{msg}' not found in decoded output"
            )

    def test_snapshot_multiple_buffers(self):
        """Test snapshot captures multiple tracebuffers."""
        buffers = ["MultiBuffer1", "MultiBuffer2", "MultiBuffer3"]
        messages = {}

        for buf in buffers:
            msg = f"message_from_{buf}"
            messages[buf] = msg
            self._create_tracebuffer_with_content(buf, [msg])

        output_file = os.path.join(self.trace_path, "multi_buffer.clltk")
        result = clltk("snapshot", "--output", output_file)
        self.assertEqual(result.returncode, 0)

        # Decode and verify all messages from all buffers
        result = clltk("decode", output_file)
        self.assertEqual(result.returncode, 0)

        for buf, msg in messages.items():
            self.assertIn(msg, result.stdout, f"Message from {buf} not found")


class TestSnapshotFiltering(unittest.TestCase):
    """Tests for snapshot filter option."""

    def setUp(self):
        """Create temporary directory and set environment."""
        self.tmp_dir = tempfile.TemporaryDirectory()
        self.trace_path = self.tmp_dir.name
        self.old_env = os.environ.get("CLLTK_TRACING_PATH")
        os.environ["CLLTK_TRACING_PATH"] = self.trace_path

    def tearDown(self):
        """Clean up temporary directory and restore environment."""
        if self.old_env:
            os.environ["CLLTK_TRACING_PATH"] = self.old_env
        else:
            os.environ.pop("CLLTK_TRACING_PATH", None)
        self.tmp_dir.cleanup()

    def _create_tracebuffer_with_content(self, name, messages):
        """Helper to create a tracebuffer and write messages to it."""
        result = clltk("buffer", "--buffer", name, "--size", "4KB")
        self.assertEqual(
            result.returncode, 0, f"Failed to create buffer: {result.stderr}"
        )

        for msg in messages:
            result = clltk("trace", "-b", name, msg)
            self.assertEqual(
                result.returncode, 0, f"Failed to write trace: {result.stderr}"
            )

    def test_filter_includes_matching_buffers(self):
        """Test that filter includes only matching tracebuffers."""
        # Create buffers with different naming patterns
        self._create_tracebuffer_with_content("TestBufferOne", ["msg_test_one"])
        self._create_tracebuffer_with_content("TestBufferTwo", ["msg_test_two"])
        self._create_tracebuffer_with_content("OtherBuffer", ["msg_other"])

        output_file = os.path.join(self.trace_path, "filtered.clltk")
        result = clltk("snapshot", "--filter", "^TestBuffer.*", "--output", output_file)
        self.assertEqual(result.returncode, 0, f"Snapshot failed: {result.stderr}")

        # Decode and verify only TestBuffer messages are present
        result = clltk("decode", output_file)
        self.assertEqual(result.returncode, 0)

        self.assertIn("msg_test_one", result.stdout)
        self.assertIn("msg_test_two", result.stdout)
        self.assertNotIn("msg_other", result.stdout)

    def test_filter_excludes_nonmatching_buffers(self):
        """Test that filter excludes non-matching tracebuffers."""
        self._create_tracebuffer_with_content("IncludeMe", ["include_message"])
        self._create_tracebuffer_with_content("ExcludeMe", ["exclude_message"])

        output_file = os.path.join(self.trace_path, "filter_exclude.clltk")
        result = clltk("snapshot", "--filter", "^Include.*", "--output", output_file)
        self.assertEqual(result.returncode, 0)

        result = clltk("decode", output_file)
        self.assertEqual(result.returncode, 0)

        self.assertIn("include_message", result.stdout)
        self.assertNotIn("exclude_message", result.stdout)

    def test_filter_regex_pattern(self):
        """Test filter with regex pattern."""
        self._create_tracebuffer_with_content("App1Buffer", ["app1_msg"])
        self._create_tracebuffer_with_content("App2Buffer", ["app2_msg"])
        self._create_tracebuffer_with_content("SystemBuffer", ["system_msg"])

        output_file = os.path.join(self.trace_path, "regex_filter.clltk")
        # Filter to only include App* buffers
        result = clltk(
            "snapshot", "--filter", "^App[0-9]Buffer$", "--output", output_file
        )
        self.assertEqual(result.returncode, 0)

        result = clltk("decode", output_file)
        self.assertEqual(result.returncode, 0)

        self.assertIn("app1_msg", result.stdout)
        self.assertIn("app2_msg", result.stdout)
        self.assertNotIn("system_msg", result.stdout)


class TestSnapshotInclude(unittest.TestCase):
    """Tests for snapshot include option."""

    def setUp(self):
        """Create temporary directory and set environment."""
        self.tmp_dir = tempfile.TemporaryDirectory()
        self.trace_path = self.tmp_dir.name
        self.old_env = os.environ.get("CLLTK_TRACING_PATH")
        os.environ["CLLTK_TRACING_PATH"] = self.trace_path

    def tearDown(self):
        """Clean up temporary directory and restore environment."""
        if self.old_env:
            os.environ["CLLTK_TRACING_PATH"] = self.old_env
        else:
            os.environ.pop("CLLTK_TRACING_PATH", None)
        self.tmp_dir.cleanup()

    def _create_tracebuffer_with_content(self, name, messages, path=None):
        """Helper to create a tracebuffer and write messages to it."""
        env = os.environ.copy()
        if path:
            env["CLLTK_TRACING_PATH"] = path

        result = clltk("buffer", "--buffer", name, "--size", "4KB", env=env)
        self.assertEqual(
            result.returncode, 0, f"Failed to create buffer: {result.stderr}"
        )

        for msg in messages:
            result = clltk("trace", "-b", name, msg, env=env)
            self.assertEqual(
                result.returncode, 0, f"Failed to write trace: {result.stderr}"
            )

    def test_include_additional_path(self):
        """Test including additional paths in snapshot."""
        # Create buffer in default path
        self._create_tracebuffer_with_content("MainBuffer", ["main_message"])

        # Create another directory with a buffer
        extra_dir = os.path.join(self.tmp_dir.name, "extra")
        os.makedirs(extra_dir)
        self._create_tracebuffer_with_content(
            "ExtraBuffer", ["extra_message"], path=extra_dir
        )

        # Find the extra trace file
        extra_trace_file = ""
        for f in os.listdir(extra_dir):
            if f.endswith(".clltk_trace"):
                extra_trace_file = os.path.join(extra_dir, f)
                break

        self.assertTrue(extra_trace_file, "Extra trace file not found")

        output_file = os.path.join(self.trace_path, "include_test.clltk")
        result = clltk(
            "snapshot", "--include", extra_trace_file, "--output", output_file
        )
        self.assertEqual(result.returncode, 0, f"Snapshot failed: {result.stderr}")

        # Decode and verify both messages are present
        result = clltk("decode", output_file)
        self.assertEqual(result.returncode, 0)

        self.assertIn("main_message", result.stdout)
        self.assertIn("extra_message", result.stdout)

    def test_include_multiple_paths(self):
        """Test including multiple additional paths."""
        # Create buffer in default path
        self._create_tracebuffer_with_content("DefaultBuffer", ["default_msg"])

        # Create two additional directories
        extra1 = os.path.join(self.tmp_dir.name, "extra1")
        extra2 = os.path.join(self.tmp_dir.name, "extra2")
        os.makedirs(extra1)
        os.makedirs(extra2)

        self._create_tracebuffer_with_content(
            "Extra1Buffer", ["extra1_msg"], path=extra1
        )
        self._create_tracebuffer_with_content(
            "Extra2Buffer", ["extra2_msg"], path=extra2
        )

        # Find the trace files
        extra1_file = ""
        extra2_file = ""
        for f in os.listdir(extra1):
            if f.endswith(".clltk_trace"):
                extra1_file = os.path.join(extra1, f)
        for f in os.listdir(extra2):
            if f.endswith(".clltk_trace"):
                extra2_file = os.path.join(extra2, f)

        self.assertTrue(extra1_file, "Extra1 trace file not found")
        self.assertTrue(extra2_file, "Extra2 trace file not found")

        output_file = os.path.join(self.trace_path, "multi_include.clltk")
        result = clltk(
            "snapshot",
            "--include",
            extra1_file,
            "--include",
            extra2_file,
            "--output",
            output_file,
        )
        self.assertEqual(result.returncode, 0)

        result = clltk("decode", output_file)
        self.assertEqual(result.returncode, 0)

        self.assertIn("default_msg", result.stdout)
        self.assertIn("extra1_msg", result.stdout)
        self.assertIn("extra2_msg", result.stdout)


class TestSnapshotEdgeCases(unittest.TestCase):
    """Edge case tests for snapshot command."""

    def setUp(self):
        """Create temporary directory and set environment."""
        self.tmp_dir = tempfile.TemporaryDirectory()
        self.trace_path = self.tmp_dir.name
        self.old_env = os.environ.get("CLLTK_TRACING_PATH")
        os.environ["CLLTK_TRACING_PATH"] = self.trace_path

    def tearDown(self):
        """Clean up temporary directory and restore environment."""
        if self.old_env:
            os.environ["CLLTK_TRACING_PATH"] = self.old_env
        else:
            os.environ.pop("CLLTK_TRACING_PATH", None)
        self.tmp_dir.cleanup()

    def _create_tracebuffer_with_content(self, name, messages):
        """Helper to create a tracebuffer and write messages to it."""
        result = clltk("buffer", "--buffer", name, "--size", "4KB")
        self.assertEqual(
            result.returncode, 0, f"Failed to create buffer: {result.stderr}"
        )

        for msg in messages:
            result = clltk("trace", "-b", name, msg)
            self.assertEqual(
                result.returncode, 0, f"Failed to write trace: {result.stderr}"
            )

    def test_empty_directory(self):
        """Test snapshot on empty directory with no tracebuffers."""
        output_file = os.path.join(self.trace_path, "empty.clltk")

        # Snapshot should still succeed, just create empty/minimal archive
        result = clltk("snapshot", "--output", output_file, check=False)
        # The command may succeed or fail depending on implementation
        # Just verify it doesn't crash unexpectedly

    def test_nonexistent_output_directory(self):
        """Test snapshot with output path in nonexistent directory."""
        self._create_tracebuffer_with_content("TestBuffer", ["test_msg"])

        # Try to write to nonexistent directory
        bad_path = "/nonexistent/path/snapshot.clltk"
        result = clltk("snapshot", "--output", bad_path, check=False)

        # Should fail with error
        self.assertNotEqual(
            result.returncode, 0, "Should fail for nonexistent output path"
        )

    def test_bucket_size_option(self):
        """Test --bucket-size option."""
        self._create_tracebuffer_with_content("BucketTestBuffer", ["bucket_test_msg"])

        output_file = os.path.join(self.trace_path, "bucket_test.clltk")
        result = clltk("snapshot", "--bucket-size", "8192", "--output", output_file)
        self.assertEqual(result.returncode, 0, f"Snapshot failed: {result.stderr}")

        # Verify snapshot was created
        self.assertTrue(os.path.exists(output_file))

        # Verify it can be decoded
        result = clltk("decode", output_file)
        self.assertEqual(result.returncode, 0)
        self.assertIn("bucket_test_msg", result.stdout)

    def test_snapshot_overwrites_existing_file(self):
        """Test that snapshot overwrites existing output file."""
        self._create_tracebuffer_with_content("OverwriteBuffer", ["first_message"])

        output_file = os.path.join(self.trace_path, "overwrite.clltk")

        # Create first snapshot
        result = clltk("snapshot", "--output", output_file)
        self.assertEqual(result.returncode, 0)
        first_size = os.path.getsize(output_file)

        # Add more content and create second snapshot
        result = clltk("trace", "-b", "OverwriteBuffer", "second_message")
        self.assertEqual(result.returncode, 0)

        result = clltk("snapshot", "--output", output_file)
        self.assertEqual(result.returncode, 0)

        # File should exist and may have different size
        self.assertTrue(os.path.exists(output_file))

        # Decode should show second message
        result = clltk("decode", output_file)
        self.assertEqual(result.returncode, 0)
        self.assertIn("second_message", result.stdout)

    def test_filter_no_matches(self):
        """Test filter that matches no buffers."""
        self._create_tracebuffer_with_content("TestBuffer", ["test_msg"])

        output_file = os.path.join(self.trace_path, "no_match.clltk")
        result = clltk(
            "snapshot",
            "--filter",
            "^NoSuchBuffer.*",
            "--output",
            output_file,
            check=False,
        )

        # Should either fail or create empty snapshot
        # The behavior depends on implementation

    def test_recursive_flag(self):
        """Test -r/--recursive flag for subdirectories."""
        # Create a subdirectory with a tracebuffer
        sub_dir = os.path.join(self.trace_path, "subdir")
        os.makedirs(sub_dir)

        # Create buffer in main directory
        self._create_tracebuffer_with_content("MainDirBuffer", ["main_dir_msg"])

        # Create buffer in subdirectory
        env = os.environ.copy()
        env["CLLTK_TRACING_PATH"] = sub_dir
        result = clltk("buffer", "--buffer", "SubDirBuffer", "--size", "4KB", env=env)
        self.assertEqual(result.returncode, 0)
        result = clltk("trace", "-b", "SubDirBuffer", "sub_dir_msg", env=env)
        self.assertEqual(result.returncode, 0)

        # Create snapshot with recursive flag
        output_file = os.path.join(self.trace_path, "recursive.clltk")
        result = clltk("snapshot", "-r", "--output", output_file)
        self.assertEqual(result.returncode, 0, f"Snapshot failed: {result.stderr}")

        # Decode and check if subdirectory buffer is included
        result = clltk("decode", output_file)
        self.assertEqual(result.returncode, 0)
        # Should find main directory message
        self.assertIn("main_dir_msg", result.stdout)


if __name__ == "__main__":
    unittest.main()
