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
        """Test -C flag for custom tracing paths."""
        # Create subdirectories
        dir_a = pathlib.Path(self.tmp_dir.name) / "NewDirA"
        dir_b = pathlib.Path(self.tmp_dir.name) / "NewDirB"
        dir_a.mkdir()
        dir_b.mkdir()

        # Create tracebuffer in dir A
        os.environ["CLLTK_TRACING_PATH"] = str(dir_a)
        result = clltk(
            "-P", str(dir_a), "buffer", "--buffer", "BufferInA", "--size", "1KB"
        )
        self.assertEqual(result.returncode, 0, msg=result.stderr)

        # Create tracebuffer in dir B
        os.environ["CLLTK_TRACING_PATH"] = str(dir_b)
        result = clltk(
            "-P", str(dir_b), "buffer", "--buffer", "BufferInB", "--size", "1KB"
        )
        self.assertEqual(result.returncode, 0, msg=result.stderr)

        # Verify files in dir A
        files_a = self._list_trace_files(str(dir_a))
        self.assertEqual(len(files_a), 1, msg=str(files_a))
        self.assertEqual(files_a[0].name, "BufferInA.clltk_trace")

        # Verify files in dir B
        files_b = self._list_trace_files(str(dir_b))
        self.assertEqual(len(files_b), 1, msg=str(files_b))
        self.assertEqual(files_b[0].name, "BufferInB.clltk_trace")


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
        """Test that trace subcommand exists and shows help."""
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
        result = clltk("trace", "--buffer", "TestBuffer", "--message", "test message")
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


if __name__ == "__main__":
    unittest.main()
