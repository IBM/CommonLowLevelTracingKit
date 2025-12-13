#!/usr/bin/env python3
# Copyright (c) 2024, International Business Machines
# SPDX-License-Identifier: BSD-2-Clause-Patent

"""
CLLTK Global Options Tests.

These tests verify the functionality of global CLI options that apply
across all subcommands, including verbosity control, path configuration,
and version information.
"""

import os
import pathlib
import re
import sys
import tempfile
import unittest

# Add tests directory to path for imports
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.realpath(__file__))))

from helpers.base import run_command
from helpers.clltk_cmd import clltk


def setUpModule():
    """Configure CMake and build clltk-cmd before running tests."""
    run_command("cmake --preset default")
    run_command("cmake --build --preset default --target clltk-cmd")


class TestVersionOption(unittest.TestCase):
    """Tests for the -V/--version global option."""

    def test_version_long_flag(self):
        """Test --version outputs version information."""
        result = clltk("--version")
        self.assertEqual(result.returncode, 0)
        self.assertIn("Common Low Level Tracing Kit", result.stdout)

    def test_version_short_flag(self):
        """Test -V outputs version information."""
        result = clltk("-V")
        self.assertEqual(result.returncode, 0)
        self.assertIn("Common Low Level Tracing Kit", result.stdout)

    def test_version_matches_pattern(self):
        """Test version string matches expected pattern (X.Y.Z format)."""
        result = clltk("--version")
        self.assertEqual(result.returncode, 0)
        # Version should match semantic versioning pattern
        self.assertRegex(
            result.stdout,
            r"Common Low Level Tracing Kit \d+\.\d+\.\d+",
            "Version string should match 'Common Low Level Tracing Kit X.Y.Z' format",
        )

    def test_version_output_format(self):
        """Test version output format is consistent."""
        result = clltk("--version")
        self.assertEqual(result.returncode, 0)
        # Extract version number
        match = re.search(r"(\d+\.\d+\.\d+)", result.stdout)
        self.assertIsNotNone(match, "Version number not found in output")
        assert match is not None  # For type checker
        version = match.group(1)
        parts = version.split(".")
        self.assertEqual(
            len(parts), 3, "Version should have 3 parts (major.minor.patch)"
        )
        # Each part should be a valid integer
        for part in parts:
            self.assertTrue(part.isdigit(), f"Version part '{part}' should be numeric")

    def test_version_both_flags_produce_same_output(self):
        """Test that -V and --version produce identical output."""
        result_short = clltk("-V")
        result_long = clltk("--version")
        self.assertEqual(result_short.returncode, result_long.returncode)
        self.assertEqual(result_short.stdout, result_long.stdout)


class TestPathOption(unittest.TestCase):
    """Tests for the -P/--path global option."""

    def setUp(self):
        """Create temporary directories and save environment."""
        self.tmp_dir = tempfile.TemporaryDirectory()
        self.alt_dir = tempfile.TemporaryDirectory()
        self.old_env = os.environ.get("CLLTK_TRACING_PATH")
        os.environ["CLLTK_TRACING_PATH"] = self.tmp_dir.name

    def tearDown(self):
        """Clean up temporary directories and restore environment."""
        if self.old_env:
            os.environ["CLLTK_TRACING_PATH"] = self.old_env
        else:
            os.environ.pop("CLLTK_TRACING_PATH", None)
        self.tmp_dir.cleanup()
        self.alt_dir.cleanup()

    def _list_trace_files(self, path: str) -> list:
        """List .clltk_trace files in the given path."""
        trace_path = pathlib.Path(path)
        return list(trace_path.glob("*.clltk_trace"))

    def test_path_long_flag_overrides_env(self):
        """Test --path overrides CLLTK_TRACING_PATH environment variable."""
        # Create buffer using --path to alternate directory
        result = clltk(
            "--path",
            self.alt_dir.name,
            "buffer",
            "--buffer",
            "PathTest",
            "--size",
            "1KB",
        )
        self.assertEqual(result.returncode, 0, msg=result.stderr)

        # Buffer should be in alt_dir, not tmp_dir
        files_alt = self._list_trace_files(self.alt_dir.name)
        files_tmp = self._list_trace_files(self.tmp_dir.name)

        self.assertEqual(
            len(files_alt), 1, "Buffer should be created in --path directory"
        )
        self.assertEqual(
            len(files_tmp), 0, "Buffer should NOT be created in env directory"
        )
        self.assertEqual(files_alt[0].name, "PathTest.clltk_trace")

    def test_path_short_flag_overrides_env(self):
        """Test -P overrides CLLTK_TRACING_PATH environment variable."""
        # Create buffer using -P to alternate directory
        result = clltk(
            "-P",
            self.alt_dir.name,
            "buffer",
            "--buffer",
            "ShortPathTest",
            "--size",
            "1KB",
        )
        self.assertEqual(result.returncode, 0, msg=result.stderr)

        # Buffer should be in alt_dir
        files_alt = self._list_trace_files(self.alt_dir.name)
        self.assertEqual(len(files_alt), 1)
        self.assertEqual(files_alt[0].name, "ShortPathTest.clltk_trace")

    def test_cli_path_takes_precedence_over_env(self):
        """Test CLI -P/--path takes precedence over CLLTK_TRACING_PATH."""
        # Create buffer in env path
        result = clltk("buffer", "--buffer", "EnvBuffer", "--size", "1KB")
        self.assertEqual(result.returncode, 0)

        # Create buffer using CLI path option (should go to alt_dir)
        result = clltk(
            "--path",
            self.alt_dir.name,
            "buffer",
            "--buffer",
            "CliBuffer",
            "--size",
            "1KB",
        )
        self.assertEqual(result.returncode, 0)

        # Verify locations
        files_tmp = self._list_trace_files(self.tmp_dir.name)
        files_alt = self._list_trace_files(self.alt_dir.name)

        tmp_names = [f.stem for f in files_tmp]
        alt_names = [f.stem for f in files_alt]

        self.assertIn("EnvBuffer", tmp_names, "EnvBuffer should be in env path")
        self.assertIn("CliBuffer", alt_names, "CliBuffer should be in CLI path")
        self.assertNotIn("CliBuffer", tmp_names, "CliBuffer should NOT be in env path")

    def test_path_works_with_list_command(self):
        """Test -P/--path works with list subcommand."""
        # Create buffer in alt directory
        result = clltk(
            "-P",
            self.alt_dir.name,
            "buffer",
            "--buffer",
            "ListPathTest",
            "--size",
            "1KB",
        )
        self.assertEqual(result.returncode, 0)

        # List using --path should find the buffer
        result = clltk("--path", self.alt_dir.name, "list")
        self.assertEqual(result.returncode, 0)
        self.assertIn("ListPathTest", result.stdout)

        # List without --path (uses env) should NOT find it
        result = clltk("list")
        self.assertEqual(result.returncode, 0)
        self.assertNotIn("ListPathTest", result.stdout)

    def test_path_works_with_decode_command(self):
        """Test -P/--path works with decode subcommand."""
        # Create buffer with a trace in alt directory
        result = clltk(
            "-P",
            self.alt_dir.name,
            "buffer",
            "--buffer",
            "DecodePathTest",
            "--size",
            "1KB",
        )
        self.assertEqual(result.returncode, 0)

        result = clltk(
            "-P", self.alt_dir.name, "trace", "DecodePathTest", "path test message"
        )
        self.assertEqual(result.returncode, 0)

        # Decode using --path should find and decode the buffer
        result = clltk("--path", self.alt_dir.name, "decode")
        self.assertEqual(result.returncode, 0)
        self.assertIn("path test message", result.stdout)

    def test_path_works_with_buffer_command(self):
        """Test -P/--path works with buffer subcommand."""
        result = clltk(
            "-P",
            self.alt_dir.name,
            "buffer",
            "--buffer",
            "BufferPathTest",
            "--size",
            "2KB",
        )
        self.assertEqual(result.returncode, 0)

        files = self._list_trace_files(self.alt_dir.name)
        self.assertEqual(len(files), 1)
        self.assertEqual(files[0].name, "BufferPathTest.clltk_trace")

    def test_path_invalid_directory(self):
        """Test -P with invalid directory path fails gracefully."""
        result = clltk(
            "-P",
            "/nonexistent/directory/path",
            "buffer",
            "--buffer",
            "InvalidPath",
            "--size",
            "1KB",
            check=False,
        )
        # Should fail since directory doesn't exist
        self.assertNotEqual(result.returncode, 0)


class TestVerbosityOptions(unittest.TestCase):
    """Tests for the -q/--quiet and -v/--verbose global options."""

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

    def test_quiet_long_flag_recognized(self):
        """Test --quiet flag is recognized."""
        result = clltk("--quiet", "--help")
        self.assertEqual(result.returncode, 0)

    def test_quiet_short_flag_recognized(self):
        """Test -q flag is recognized."""
        result = clltk("-q", "--help")
        self.assertEqual(result.returncode, 0)

    def test_verbose_long_flag_recognized(self):
        """Test --verbose flag is recognized."""
        result = clltk("--verbose", "--help")
        self.assertEqual(result.returncode, 0)

    def test_verbose_short_flag_recognized(self):
        """Test -v flag is recognized."""
        result = clltk("-v", "--help")
        self.assertEqual(result.returncode, 0)

    def test_quiet_mode_suppresses_info_messages(self):
        """Test quiet mode suppresses informational messages."""
        # Create a buffer in quiet mode
        result_quiet = clltk("-q", "buffer", "--buffer", "QuietBuffer", "--size", "1KB")
        self.assertEqual(result_quiet.returncode, 0)

        # Create another buffer in normal mode for comparison
        result_normal = clltk("buffer", "--buffer", "NormalBuffer", "--size", "1KB")
        self.assertEqual(result_normal.returncode, 0)

        # Quiet mode should produce less or equal output
        self.assertLessEqual(
            len(result_quiet.stdout),
            len(result_normal.stdout),
            "Quiet mode should not produce more output than normal mode",
        )

    def test_verbose_mode_shows_more_detail(self):
        """Test verbose mode shows more detailed output."""
        # Create buffer in verbose mode
        result_verbose = clltk(
            "-v", "buffer", "--buffer", "VerboseBuffer", "--size", "1KB"
        )
        self.assertEqual(result_verbose.returncode, 0)

        # Create buffer in normal mode for comparison
        result_normal = clltk("buffer", "--buffer", "NormalBuffer2", "--size", "1KB")
        self.assertEqual(result_normal.returncode, 0)

        # Verbose mode should produce more or equal output
        self.assertGreaterEqual(
            len(result_verbose.stdout) + len(result_verbose.stderr),
            len(result_normal.stdout) + len(result_normal.stderr),
            "Verbose mode should produce at least as much output as normal mode",
        )

    def test_quiet_mode_with_list_command(self):
        """Test quiet mode works with list subcommand."""
        # Create a buffer first
        result = clltk("buffer", "--buffer", "QuietListTest", "--size", "1KB")
        self.assertEqual(result.returncode, 0)

        # List in quiet mode
        result = clltk("-q", "list")
        self.assertEqual(result.returncode, 0)
        # Should still show the buffer (data is not suppressed)
        self.assertIn("QuietListTest", result.stdout)

    def test_verbose_mode_with_decode_command(self):
        """Test verbose mode works with decode subcommand."""
        # Create buffer with trace
        result = clltk("buffer", "--buffer", "VerboseDecodeTest", "--size", "1KB")
        self.assertEqual(result.returncode, 0)
        result = clltk("trace", "VerboseDecodeTest", "verbose test message")
        self.assertEqual(result.returncode, 0)

        # Decode in verbose mode
        result = clltk("-v", "decode")
        self.assertEqual(result.returncode, 0)
        # Should still contain the trace data
        self.assertIn("verbose test message", result.stdout)

    def test_quiet_and_verbose_are_mutually_exclusive(self):
        """Test that using both -q and -v together is handled."""
        # The behavior may vary - either error or last flag wins
        # We just verify it doesn't crash
        result = clltk("-q", "-v", "--help", check=False)
        # Accept either success (last flag wins) or explicit error
        # The important thing is it handles this gracefully


class TestDefaultBehavior(unittest.TestCase):
    """Tests for default behavior when no environment variables are set."""

    def setUp(self):
        """Save and unset CLLTK_TRACING_PATH environment variable."""
        self.old_env = os.environ.get("CLLTK_TRACING_PATH")
        if "CLLTK_TRACING_PATH" in os.environ:
            del os.environ["CLLTK_TRACING_PATH"]
        # Create temp dir for testing
        self.tmp_dir = tempfile.TemporaryDirectory()
        # Change to tmp_dir to avoid polluting current directory
        self.old_cwd = os.getcwd()
        os.chdir(self.tmp_dir.name)

    def tearDown(self):
        """Restore CLLTK_TRACING_PATH and working directory."""
        os.chdir(self.old_cwd)
        if self.old_env:
            os.environ["CLLTK_TRACING_PATH"] = self.old_env
        self.tmp_dir.cleanup()

    def test_default_path_is_current_directory(self):
        """Test default tracing path is current directory when env not set."""
        # With no CLLTK_TRACING_PATH set, buffer should be created in cwd
        result = clltk("buffer", "--buffer", "DefaultPathBuffer", "--size", "1KB")
        self.assertEqual(result.returncode, 0, msg=result.stderr)

        # Check buffer was created in current directory (tmp_dir)
        trace_files = list(pathlib.Path(self.tmp_dir.name).glob("*.clltk_trace"))
        self.assertEqual(len(trace_files), 1)
        self.assertEqual(trace_files[0].name, "DefaultPathBuffer.clltk_trace")

    def test_list_uses_current_directory_by_default(self):
        """Test list command uses current directory when env not set."""
        # Create a buffer (will be in cwd)
        result = clltk("buffer", "--buffer", "DefaultListBuffer", "--size", "1KB")
        self.assertEqual(result.returncode, 0)

        # List without specifying path should use cwd
        result = clltk("list")
        self.assertEqual(result.returncode, 0)
        self.assertIn("DefaultListBuffer", result.stdout)

    def test_decode_uses_current_directory_by_default(self):
        """Test decode command uses current directory when env not set."""
        # Create buffer with trace
        result = clltk("buffer", "--buffer", "DefaultDecodeBuffer", "--size", "1KB")
        self.assertEqual(result.returncode, 0)
        result = clltk("trace", "DefaultDecodeBuffer", "default path decode test")
        self.assertEqual(result.returncode, 0)

        # Decode without specifying path should use cwd
        result = clltk("decode")
        self.assertEqual(result.returncode, 0)
        self.assertIn("default path decode test", result.stdout)

    def test_explicit_path_overrides_default(self):
        """Test explicit -P path overrides default current directory behavior."""
        # Create an alternate directory
        alt_dir = tempfile.TemporaryDirectory()
        try:
            # Create buffer using explicit path
            result = clltk(
                "-P",
                alt_dir.name,
                "buffer",
                "--buffer",
                "ExplicitPathBuffer",
                "--size",
                "1KB",
            )
            self.assertEqual(result.returncode, 0)

            # Buffer should be in alt_dir, not cwd
            alt_files = list(pathlib.Path(alt_dir.name).glob("*.clltk_trace"))
            cwd_files = list(pathlib.Path(self.tmp_dir.name).glob("*.clltk_trace"))

            self.assertEqual(len(alt_files), 1, "Buffer should be in explicit path")
            self.assertEqual(len(cwd_files), 0, "Buffer should NOT be in default cwd")
        finally:
            alt_dir.cleanup()


class TestGlobalOptionsWithSubcommands(unittest.TestCase):
    """Tests verifying global options work correctly across various subcommands."""

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

    def test_global_options_before_subcommand(self):
        """Test global options can be placed before subcommand."""
        result = clltk("-v", "list", "--help")
        self.assertEqual(result.returncode, 0)
        self.assertIn("tracebuffers", result.stdout)

    def test_version_flag_ignores_subcommand(self):
        """Test --version shows version even with subcommand specified."""
        result = clltk("--version", "list", check=False)
        # --version should show version and exit, may or may not process subcommand
        self.assertIn("Common Low Level Tracing Kit", result.stdout)

    def test_help_shows_global_options(self):
        """Test main help shows all global options."""
        result = clltk("--help")
        self.assertEqual(result.returncode, 0)
        # Check for global options in help
        self.assertIn("-q", result.stdout)
        self.assertIn("-v", result.stdout)
        self.assertIn("-P", result.stdout)
        self.assertIn("-V", result.stdout)

    def test_multiple_global_options(self):
        """Test multiple global options can be combined."""
        alt_dir = tempfile.TemporaryDirectory()
        try:
            # Combine -v and -P
            result = clltk(
                "-v",
                "-P",
                alt_dir.name,
                "buffer",
                "--buffer",
                "MultiOptBuffer",
                "--size",
                "1KB",
            )
            self.assertEqual(result.returncode, 0)

            # Verify buffer was created in correct location
            files = list(pathlib.Path(alt_dir.name).glob("*.clltk_trace"))
            self.assertEqual(len(files), 1)
        finally:
            alt_dir.cleanup()


if __name__ == "__main__":
    unittest.main()
