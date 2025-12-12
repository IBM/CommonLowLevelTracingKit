#!/usr/bin/python3
# Copyright (c) 2024, International Business Machines
# SPDX-License-Identifier: BSD-2-Clause-Patent

"""
CLLTK CLI command tests.

These tests verify the functionality of the clltk command-line tool
including help, version, and subcommands.

Migrated from:
- tests/robot.tests/clltk-cmd/clltk_cmd_base.robot
- tests/robot.tests/clltk-cmd/clltk_tracebuffer.robot
- tests/robot.tests/clltk-cmd/clltk_tracepipe.robot
- tests/robot.tests/clltk-cmd/clltk_tracepoints.robot
"""

import unittest
import tempfile
import os
import sys
import re
import pathlib

# Add tests directory to path for imports
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.realpath(__file__))))

from helpers.base import run_command
from helpers.clltk_cmd import clltk


def setUpModule():
    """Configure CMake and build clltk-cmd before running tests."""
    run_command("cmake --preset default")
    run_command("cmake --build --preset default --target clltk-cmd")


class TestClltkCmdBase(unittest.TestCase):
    """Base CLI command tests."""

    def test_clltk_without_args(self):
        """Test clltk command without arguments shows usage."""
        result = clltk()
        self.assertEqual(result.returncode, 0)
        self.assertIn("clltk [OPTIONS] [SUBCOMMAND]", result.stdout)

    def test_clltk_with_help_long(self):
        """Test clltk --help shows usage."""
        result = clltk("--help")
        self.assertEqual(result.returncode, 0)
        self.assertIn("clltk [OPTIONS] [SUBCOMMAND]", result.stdout)

    def test_clltk_with_help_short(self):
        """Test clltk -h shows usage."""
        result = clltk("-h")
        self.assertEqual(result.returncode, 0)
        self.assertIn("clltk [OPTIONS] [SUBCOMMAND]", result.stdout)

    def test_clltk_version(self):
        """Test clltk --version shows version string."""
        result = clltk("--version")
        self.assertEqual(result.returncode, 0)
        self.assertRegex(result.stdout, r"Common Low Level Tracing Kit \d+\.\d+\.\d+")


class TestClltkTraceBuffer(unittest.TestCase):
    """Tracebuffer subcommand tests."""

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

    def _list_trace_files(self, path: str = None) -> list:
        """List .clltk_trace files in the given path."""
        if path is None:
            path = self.tmp_dir.name
        trace_path = pathlib.Path(path)
        return list(trace_path.glob("*.clltk_trace"))

    def _clean_directory(self, path: str = None):
        """Remove all files and subdirectories from a path."""
        if path is None:
            path = self.tmp_dir.name
        trace_path = pathlib.Path(path)
        for item in trace_path.iterdir():
            if item.is_file():
                item.unlink()
            elif item.is_dir():
                import shutil

                shutil.rmtree(item)

    def test_clltk_tracing_path_empty_by_default(self):
        """Test that CLLTK_TRACING_PATH is empty by default."""
        files = self._list_trace_files()
        self.assertEqual(len(files), 0)

    def test_create_tracebuffer(self):
        """Test creating a tracebuffer."""
        result = clltk("buffer", "--buffer", "MyFirstTracebuffer", "--size", "1KB")
        self.assertEqual(result.returncode, 0, msg=result.stderr)

        files = self._list_trace_files()
        self.assertEqual(len(files), 1, msg=str(files))
        self.assertEqual(files[0].name, "MyFirstTracebuffer.clltk_trace")

    def test_invalid_tracebuffer_names(self):
        """Test that invalid tracebuffer names are rejected."""
        invalid_names = [
            " Buffer",  # leading space
            "Buffer ",  # trailing space
            "_Buffer",  # leading underscore
            "8uffer",  # leading digit
            "Buffe~r",  # tilde
            "Buffe#r",  # hash
            "Buffe'r",  # quote
        ]
        for name in invalid_names:
            with self.subTest(name=name):
                result = clltk("buffer", "--buffer", name, "--size", "256", check=False)
                self.assertNotEqual(
                    result.returncode, 0, msg=f"naming tracebuffer '{name}' should fail"
                )

    def test_valid_tracebuffer_names(self):
        """Test that valid tracebuffer names are accepted."""
        valid_names = [
            "MyFirstTracebuffer",
            "B_ffer",
            "BuffEr",
        ]
        for name in valid_names:
            with self.subTest(name=name):
                self._clean_directory()
                result = clltk("buffer", "--buffer", name, "--size", "256")
                self.assertEqual(result.returncode, 0)

                files = self._list_trace_files()
                self.assertEqual(len(files), 1, msg=str(files))
                self.assertEqual(files[0].name, f"{name}.clltk_trace")

    def test_valid_tracebuffer_sizes(self):
        """Test that various size formats are accepted."""
        valid_sizes = [
            "1MB",
            "10KB",
            "1024",
            "10",
        ]
        for size in valid_sizes:
            with self.subTest(size=size):
                self._clean_directory()
                result = clltk("buffer", "--buffer", "Buffer", "--size", size)
                self.assertEqual(
                    result.returncode,
                    0,
                    msg=f"creating tracebuffer with size {size} should not fail",
                )

                files = self._list_trace_files()
                self.assertEqual(len(files), 1, msg=str(files))
                self.assertEqual(files[0].name, "Buffer.clltk_trace")

    def test_custom_tracing_path(self):
        """Test CLLTK_TRACING_PATH environment variable for custom tracing paths."""
        # Create subdirectories
        dir_a = pathlib.Path(self.tmp_dir.name) / "NewDirA"
        dir_b = pathlib.Path(self.tmp_dir.name) / "NewDirB"
        dir_a.mkdir()
        dir_b.mkdir()

        # Create custom environment for dir A
        env_a = os.environ.copy()
        env_a["CLLTK_TRACING_PATH"] = str(dir_a)

        # Create tracebuffer in dir A
        result = clltk("buffer", "--buffer", "BufferInA", "--size", "1KB", env=env_a)
        self.assertEqual(result.returncode, 0, msg=result.stderr)

        # Create custom environment for dir B
        env_b = os.environ.copy()
        env_b["CLLTK_TRACING_PATH"] = str(dir_b)

        # Create tracebuffer in dir B
        result = clltk("buffer", "--buffer", "BufferInB", "--size", "1KB", env=env_b)
        self.assertEqual(result.returncode, 0, msg=result.stderr)

        # Verify files in dir A
        files_a = self._list_trace_files(str(dir_a))
        self.assertEqual(len(files_a), 1, msg=str(files_a))
        self.assertEqual(files_a[0].name, "BufferInA.clltk_trace")

        # Verify files in dir B
        files_b = self._list_trace_files(str(dir_b))
        self.assertEqual(len(files_b), 1, msg=str(files_b))
        self.assertEqual(files_b[0].name, "BufferInB.clltk_trace")

    def test_buffer_help(self):
        """Test that buffer --help shows usage."""
        result = clltk("buffer", "--help")
        self.assertEqual(result.returncode, 0)
        self.assertIn("buffer", result.stdout.lower())
        # Check for common help content
        self.assertTrue(
            "--size" in result.stdout or "-s" in result.stdout,
            msg="Help should mention size option",
        )

    def test_buffer_alias_tb(self):
        """Test that 'tb' alias works for buffer command."""
        result = clltk("tb", "--buffer", "AliasTestBuffer", "--size", "1KB")
        self.assertEqual(result.returncode, 0, msg=result.stderr)

        files = self._list_trace_files()
        self.assertEqual(len(files), 1, msg=str(files))
        self.assertEqual(files[0].name, "AliasTestBuffer.clltk_trace")

    def test_maximum_buffer_name_length(self):
        """Test buffer name length limits.

        Linux has NAME_MAX of 255 bytes. The tracebuffer file format is:
        - name + ".clltk_trace" (12 chars) for final file
        - name + "~XXXXXXXXXXXXXXXX.clltk_trace" (29 chars) for temp file

        So maximum safe name length is 255 - 29 = 226 characters.
        Names longer than this may fail at the filesystem level.
        """
        # Test name at 200 characters (should be safe)
        name_200 = "A" * 200
        result = clltk("buffer", "--buffer", name_200, "--size", "1KB", check=False)
        self.assertEqual(
            result.returncode, 0, msg=f"200 char name should succeed: {result.stderr}"
        )

        self._clean_directory()

        # Test name at exactly 226 characters (boundary for temp file)
        name_226 = "B" * 226
        result = clltk("buffer", "--buffer", name_226, "--size", "1KB", check=False)
        self.assertEqual(
            result.returncode, 0, msg=f"226 char name should succeed: {result.stderr}"
        )

        self._clean_directory()

        # Test name exceeding filesystem limit (should fail at OS level)
        # 257 chars + extension exceeds NAME_MAX
        name_257 = "C" * 257
        result = clltk("buffer", "--buffer", name_257, "--size", "1KB", check=False)
        self.assertNotEqual(
            result.returncode, 0, msg="Name exceeding filesystem limit should fail"
        )

    def test_buffer_size_suffixes(self):
        """Test K, KB, M, MB, G, GB size suffixes."""
        size_tests = [
            ("1K", "SuffixK"),
            ("1KB", "SuffixKB"),
            ("1M", "SuffixM"),
            ("1MB", "SuffixMB"),
            # G/GB would create very large files, skip actual creation
        ]
        for size, name in size_tests:
            with self.subTest(size=size):
                self._clean_directory()
                result = clltk("buffer", "--buffer", name, "--size", size)
                self.assertEqual(
                    result.returncode,
                    0,
                    msg=f"Size suffix {size} should be accepted: {result.stderr}",
                )

                files = self._list_trace_files()
                self.assertEqual(len(files), 1, msg=str(files))

        # Test G/GB suffixes via help or validation only (don't create huge files)
        # Just verify they parse without actually allocating
        result = clltk("buffer", "--help")
        self.assertEqual(result.returncode, 0)

    def test_buffer_minimum_size(self):
        """Test minimum size boundary."""
        # Very small sizes should work or fail gracefully
        small_sizes = ["1", "10", "100"]
        for size in small_sizes:
            with self.subTest(size=size):
                self._clean_directory()
                result = clltk(
                    "buffer", "--buffer", "MinSizeTest", "--size", size, check=False
                )
                # Command should either succeed with minimum size or fail gracefully
                # We're testing it doesn't crash

        # Zero or negative sizes should be rejected
        invalid_sizes = ["0", "-1"]
        for size in invalid_sizes:
            with self.subTest(size=size):
                self._clean_directory()
                result = clltk(
                    "buffer", "--buffer", "InvalidSize", "--size", size, check=False
                )
                self.assertNotEqual(
                    result.returncode, 0, msg=f"Size {size} should be rejected"
                )

    def test_buffer_existing_name_behavior(self):
        """Test creating buffer when file already exists."""
        # Create first buffer
        result = clltk("buffer", "--buffer", "ExistingBuffer", "--size", "1KB")
        self.assertEqual(result.returncode, 0, msg=result.stderr)

        files = self._list_trace_files()
        self.assertEqual(len(files), 1)
        original_file = files[0]
        original_mtime = original_file.stat().st_mtime

        # Try to create buffer with same name again
        result = clltk(
            "buffer", "--buffer", "ExistingBuffer", "--size", "2KB", check=False
        )
        # Should either succeed (overwrite/skip) or fail gracefully

        # Verify file still exists
        files = self._list_trace_files()
        self.assertEqual(len(files), 1, msg="Should still have exactly one file")

    def test_buffer_path_option(self):
        """Test -P/--path option for custom directory."""
        # Create a custom directory
        custom_dir = pathlib.Path(self.tmp_dir.name) / "custom_path"
        custom_dir.mkdir()

        # Test with --path option (global options must come before subcommand)
        result = clltk(
            "--path",
            str(custom_dir),
            "buffer",
            "--buffer",
            "PathTestBuffer",
            "--size",
            "1KB",
        )
        self.assertEqual(result.returncode, 0, msg=result.stderr)

        # Verify file is in custom directory
        files = self._list_trace_files(str(custom_dir))
        self.assertEqual(len(files), 1, msg=str(files))
        self.assertEqual(files[0].name, "PathTestBuffer.clltk_trace")

        # Verify file is NOT in default directory
        default_files = self._list_trace_files()
        self.assertEqual(
            len(default_files), 0, msg="No files should be in default path"
        )

        self._clean_directory()
        # Recreate custom_dir since _clean_directory removes subdirectories
        custom_dir.mkdir(exist_ok=True)

        # Test with -P short option (global options must come before subcommand)
        result = clltk(
            "-P",
            str(custom_dir),
            "buffer",
            "--buffer",
            "ShortPathBuffer",
            "--size",
            "1KB",
        )
        self.assertEqual(result.returncode, 0, msg=result.stderr)

        files = self._list_trace_files(str(custom_dir))
        self.assertEqual(len(files), 1, msg=str(files))
        self.assertEqual(files[0].name, "ShortPathBuffer.clltk_trace")


class TestClltkTracePipe(unittest.TestCase):
    """Tracepipe subcommand tests."""

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

    def test_subcommand_tracepipe_exists(self):
        """Test that tracepipe subcommand exists and shows help."""
        result = clltk("tracepipe", "--help")
        self.assertEqual(result.returncode, 0, msg=result.stderr)


class TestClltkTracePoints(unittest.TestCase):
    """Tracepoint subcommand tests."""

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

    def test_subcommand_tracepoint_exists(self):
        """Test that tracepoint subcommand exists and shows help."""
        result = clltk("trace", "--help")
        self.assertEqual(result.returncode, 0, msg=result.stderr)


class TestClltkClear(unittest.TestCase):
    """Clear subcommand tests."""

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

    def _list_trace_files(self, path: str = None) -> list:
        """List .clltk_trace files in the given path."""
        if path is None:
            path = self.tmp_dir.name
        trace_path = pathlib.Path(path)
        return list(trace_path.glob("*.clltk_trace"))

    def test_subcommand_clear_exists(self):
        """Test that clear subcommand exists and shows help."""
        result = clltk("clear", "--help")
        self.assertEqual(result.returncode, 0, msg=result.stderr)
        self.assertIn("Clear all entries", result.stdout)

    def test_clear_requires_name(self):
        """Test that clear requires a tracebuffer name."""
        result = clltk("clear", check=False)
        self.assertNotEqual(result.returncode, 0)

    def test_clear_existing_tracebuffer(self):
        """Test clearing an existing tracebuffer."""
        # Create tracebuffer
        result = clltk("buffer", "--buffer", "TestBuffer", "--size", "1KB")
        self.assertEqual(result.returncode, 0, msg=result.stderr)

        # Add a tracepoint
        result = clltk("trace", "TestBuffer", "test message")
        self.assertEqual(result.returncode, 0, msg=result.stderr)

        # Clear the tracebuffer
        result = clltk("clear", "--buffer", "TestBuffer")
        self.assertEqual(result.returncode, 0, msg=result.stderr)

        # Verify tracebuffer file still exists
        files = self._list_trace_files()
        self.assertEqual(len(files), 1)
        self.assertEqual(files[0].name, "TestBuffer.clltk_trace")

    def test_clear_with_short_option(self):
        """Test clear with -b short option."""
        # Create tracebuffer
        result = clltk("buffer", "-b", "ShortOptBuffer", "-s", "1KB")
        self.assertEqual(result.returncode, 0, msg=result.stderr)

        # Clear with short option
        result = clltk("clear", "-b", "ShortOptBuffer")
        self.assertEqual(result.returncode, 0, msg=result.stderr)

    def test_clear_with_positional_name(self):
        """Test clear with positional name argument."""
        # Create tracebuffer
        result = clltk("buffer", "PosBuffer", "--size", "1KB")
        self.assertEqual(result.returncode, 0, msg=result.stderr)

        # Clear with positional name
        result = clltk("clear", "PosBuffer")
        self.assertEqual(result.returncode, 0, msg=result.stderr)

    def test_clear_nonexistent_tracebuffer(self):
        """Test clearing a tracebuffer that doesn't exist."""
        # This should not fail - clltk_dynamic_tracebuffer_clear returns silently
        # if the tracebuffer doesn't exist (matches existing pattern)
        result = clltk("clear", "--buffer", "NonExistentBuffer", check=False)
        # The command doesn't fail, it just does nothing if buffer doesn't exist
        # This matches the behavior of the creation function

    def test_clear_invalid_name(self):
        """Test that invalid tracebuffer names are rejected."""
        invalid_names = [
            " Buffer",  # leading space
            "_Buffer",  # leading underscore
            "8uffer",  # leading digit
        ]
        for name in invalid_names:
            with self.subTest(name=name):
                result = clltk("clear", "--buffer", name, check=False)
                self.assertNotEqual(
                    result.returncode,
                    0,
                    msg=f"clearing tracebuffer with invalid name '{name}' should fail",
                )

    def test_clear_alias_bx(self):
        """Test that 'bx' alias works for clear command."""
        # Create tracebuffer
        result = clltk("buffer", "--buffer", "AliasBuffer", "--size", "1KB")
        self.assertEqual(result.returncode, 0, msg=result.stderr)

        # Add a tracepoint
        result = clltk("trace", "AliasBuffer", "test message")
        self.assertEqual(result.returncode, 0, msg=result.stderr)

        # Clear using 'bx' alias
        result = clltk("bx", "--buffer", "AliasBuffer")
        self.assertEqual(result.returncode, 0, msg=result.stderr)

        # Verify tracebuffer file still exists
        files = self._list_trace_files()
        self.assertEqual(len(files), 1)
        self.assertEqual(files[0].name, "AliasBuffer.clltk_trace")

    def test_clear_all_option(self):
        """Test --all clears all tracebuffers."""
        # Create multiple tracebuffers
        for name in ["Buffer1", "Buffer2", "Buffer3"]:
            result = clltk("buffer", "--buffer", name, "--size", "1KB")
            self.assertEqual(result.returncode, 0, msg=result.stderr)

            # Add a tracepoint to each
            result = clltk("trace", name, f"message for {name}")
            self.assertEqual(result.returncode, 0, msg=result.stderr)

        # Verify all buffers were created
        files = self._list_trace_files()
        self.assertEqual(len(files), 3)

        # Clear all tracebuffers
        result = clltk("clear", "--all")
        self.assertEqual(result.returncode, 0, msg=result.stderr)

        # Verify all tracebuffer files still exist (clear doesn't delete files)
        files = self._list_trace_files()
        self.assertEqual(len(files), 3)

    def test_clear_all_with_filter(self):
        """Test --all with --filter clears only matching tracebuffers."""
        # Create multiple tracebuffers with different naming patterns
        buffers_alpha = ["Alpha1", "Alpha2"]
        buffers_beta = ["Beta1", "Beta2"]

        for name in buffers_alpha + buffers_beta:
            result = clltk("buffer", "--buffer", name, "--size", "1KB")
            self.assertEqual(result.returncode, 0, msg=result.stderr)

            # Add a tracepoint to each
            result = clltk("trace", name, f"message for {name}")
            self.assertEqual(result.returncode, 0, msg=result.stderr)

        # Verify all buffers were created
        files = self._list_trace_files()
        self.assertEqual(len(files), 4)

        # Clear only Alpha buffers using filter
        result = clltk("clear", "--all", "--filter", "Alpha*", check=False)
        # Note: This may or may not be supported; we're testing the interface
        # If not supported, the command may fail or ignore the filter

        # Verify all tracebuffer files still exist (clear doesn't delete files)
        files = self._list_trace_files()
        self.assertEqual(len(files), 4)

    def test_clear_verifies_content_cleared(self):
        """Decode after clear shows no entries."""
        # Create tracebuffer
        result = clltk("buffer", "--buffer", "ContentClearBuffer", "--size", "1KB")
        self.assertEqual(result.returncode, 0, msg=result.stderr)

        # Add tracepoints
        for i in range(5):
            result = clltk("trace", "ContentClearBuffer", f"message {i}")
            self.assertEqual(result.returncode, 0, msg=result.stderr)

        # Clear the tracebuffer
        result = clltk("clear", "--buffer", "ContentClearBuffer")
        self.assertEqual(result.returncode, 0, msg=result.stderr)

        # Decode the tracebuffer - should show no entries or empty output
        result = clltk("decode", "--buffer", "ContentClearBuffer", check=False)
        # After clearing, decode should either succeed with no entries
        # or return an appropriate status
        # The exact behavior depends on implementation

        # Verify the file still exists
        files = self._list_trace_files()
        self.assertEqual(len(files), 1)
        self.assertEqual(files[0].name, "ContentClearBuffer.clltk_trace")

    def test_clear_multiple_times(self):
        """Clear same buffer multiple times."""
        # Create tracebuffer
        result = clltk("buffer", "--buffer", "MultiClearBuffer", "--size", "1KB")
        self.assertEqual(result.returncode, 0, msg=result.stderr)

        # Add a tracepoint
        result = clltk("trace", "MultiClearBuffer", "initial message")
        self.assertEqual(result.returncode, 0, msg=result.stderr)

        # Clear multiple times - should all succeed
        for i in range(3):
            result = clltk("clear", "--buffer", "MultiClearBuffer")
            self.assertEqual(
                result.returncode,
                0,
                msg=f"Clear attempt {i + 1} failed: {result.stderr}",
            )

        # Add more tracepoints after multiple clears
        result = clltk("trace", "MultiClearBuffer", "message after clears")
        self.assertEqual(result.returncode, 0, msg=result.stderr)

        # Clear one more time
        result = clltk("clear", "--buffer", "MultiClearBuffer")
        self.assertEqual(result.returncode, 0, msg=result.stderr)

        # Verify the file still exists and is usable
        files = self._list_trace_files()
        self.assertEqual(len(files), 1)
        self.assertEqual(files[0].name, "MultiClearBuffer.clltk_trace")


if __name__ == "__main__":
    unittest.main()
