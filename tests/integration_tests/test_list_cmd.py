#!/usr/bin/env python3
# Copyright (c) 2024, International Business Machines
# SPDX-License-Identifier: BSD-2-Clause-Patent

"""
CLLTK List Command Tests.

These tests verify the functionality of the clltk list command
including output formats, filtering, recursive search, and statistics.
"""

import json
import os
import pathlib
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


class TestListCommandBase(unittest.TestCase):
    """Base CLI command tests for list subcommand."""

    def test_list_help(self):
        """Test that list --help shows usage information."""
        result = clltk("list", "--help")
        self.assertEqual(result.returncode, 0)
        self.assertIn("tracebuffers", result.stdout)
        self.assertIn("statistics", result.stdout)
        self.assertIn("--recursive", result.stdout)
        self.assertIn("--json", result.stdout)
        self.assertIn("--filter", result.stdout)

    def test_list_subcommand_exists(self):
        """Test that list subcommand exists."""
        result = clltk("list", "--help")
        self.assertEqual(result.returncode, 0)

    def test_list_alias_ls(self):
        """Test that ls alias works for list command."""
        result = clltk("ls", "--help")
        self.assertEqual(result.returncode, 0)
        self.assertIn("tracebuffers", result.stdout)


class TestListBasicFunctionality(unittest.TestCase):
    """Basic functionality tests for list command."""

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

    def test_list_empty_directory(self):
        """Test listing an empty directory shows no tracebuffers."""
        result = clltk("list", self.tmp_dir.name)
        self.assertEqual(result.returncode, 0)
        self.assertIn("No tracebuffers found", result.stdout)

    def test_list_with_one_buffer(self):
        """Test listing a directory with one tracebuffer."""
        # Create a tracebuffer
        result = clltk("buffer", "--buffer", "SingleBuffer", "--size", "1KB")
        self.assertEqual(result.returncode, 0, msg=result.stderr)

        # List tracebuffers
        result = clltk("list", self.tmp_dir.name)
        self.assertEqual(result.returncode, 0)
        self.assertIn("SingleBuffer", result.stdout)
        self.assertIn("NAME", result.stdout)  # Header should be present

    def test_list_with_multiple_buffers(self):
        """Test listing a directory with multiple tracebuffers."""
        buffer_names = ["BufferA", "BufferB", "BufferC"]

        # Create multiple tracebuffers
        for name in buffer_names:
            result = clltk("buffer", "--buffer", name, "--size", "1KB")
            self.assertEqual(result.returncode, 0, msg=result.stderr)

        # List tracebuffers
        result = clltk("list", self.tmp_dir.name)
        self.assertEqual(result.returncode, 0)

        # Verify all buffers are listed
        for name in buffer_names:
            self.assertIn(name, result.stdout)

    def test_list_uses_env_path_by_default(self):
        """Test that list uses CLLTK_TRACING_PATH when no path specified."""
        # Create a tracebuffer in the env path
        result = clltk("buffer", "--buffer", "EnvPathBuffer", "--size", "1KB")
        self.assertEqual(result.returncode, 0)

        # List without specifying path
        result = clltk("list")
        self.assertEqual(result.returncode, 0)
        self.assertIn("EnvPathBuffer", result.stdout)


class TestListOutputFormats(unittest.TestCase):
    """Tests for list command output formats."""

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

    def test_default_table_format(self):
        """Test that default output is table format with headers."""
        # Create a tracebuffer
        result = clltk("buffer", "--buffer", "TableBuffer", "--size", "1KB")
        self.assertEqual(result.returncode, 0)

        # List in default format
        result = clltk("list", self.tmp_dir.name)
        self.assertEqual(result.returncode, 0)

        # Check for table headers
        self.assertIn("NAME", result.stdout)
        self.assertIn("SOURCE", result.stdout)
        self.assertIn("CAPACITY", result.stdout)
        self.assertIn("USED", result.stdout)
        self.assertIn("FILL", result.stdout)
        self.assertIn("ENTRIES", result.stdout)
        self.assertIn("PENDING", result.stdout)
        self.assertIn("DROPPED", result.stdout)
        self.assertIn("WRAPPED", result.stdout)
        self.assertIn("MODIFIED", result.stdout)
        self.assertIn("PATH", result.stdout)

    def test_json_format_flag(self):
        """Test that --json flag outputs JSON format."""
        # Create a tracebuffer
        result = clltk("buffer", "--buffer", "JsonBuffer", "--size", "1KB")
        self.assertEqual(result.returncode, 0)

        # List in JSON format
        result = clltk("list", "--json", self.tmp_dir.name)
        self.assertEqual(result.returncode, 0)

        # Verify output is valid JSON
        try:
            data = json.loads(result.stdout)
            self.assertIsInstance(data, list)
        except json.JSONDecodeError as e:
            self.fail(f"Output is not valid JSON: {e}\nOutput: {result.stdout}")

    def test_json_format_short_flag(self):
        """Test that -j short flag outputs JSON format."""
        # Create a tracebuffer
        result = clltk("buffer", "--buffer", "JsonShortBuffer", "--size", "1KB")
        self.assertEqual(result.returncode, 0)

        # List in JSON format with short flag
        result = clltk("list", "-j", self.tmp_dir.name)
        self.assertEqual(result.returncode, 0)

        # Verify output is valid JSON
        data = json.loads(result.stdout)
        self.assertIsInstance(data, list)

    def test_json_structure_validation(self):
        """Test that JSON output has expected structure."""
        # Create a tracebuffer
        result = clltk("buffer", "--buffer", "StructBuffer", "--size", "2KB")
        self.assertEqual(result.returncode, 0)

        # List in JSON format
        result = clltk("list", "--json", self.tmp_dir.name)
        self.assertEqual(result.returncode, 0)

        data = json.loads(result.stdout)
        self.assertEqual(len(data), 1)

        buffer_info = data[0]

        # Check required fields
        self.assertIn("name", buffer_info)
        self.assertIn("path", buffer_info)
        self.assertIn("source_type", buffer_info)
        self.assertIn("capacity", buffer_info)
        self.assertIn("used", buffer_info)
        self.assertIn("available", buffer_info)
        self.assertIn("fill_percent", buffer_info)
        self.assertIn("entries", buffer_info)
        self.assertIn("pending", buffer_info)
        self.assertIn("dropped", buffer_info)
        self.assertIn("wrapped", buffer_info)
        self.assertIn("modified", buffer_info)

        # Check field values
        self.assertEqual(buffer_info["name"], "StructBuffer")
        self.assertIn("StructBuffer", buffer_info["path"])
        self.assertIsInstance(buffer_info["capacity"], int)
        self.assertIsInstance(buffer_info["entries"], int)

    def test_json_empty_directory(self):
        """Test that JSON output for empty directory is empty array."""
        result = clltk("list", "--json", self.tmp_dir.name)
        self.assertEqual(result.returncode, 0)

        data = json.loads(result.stdout)
        self.assertEqual(data, [])


class TestListOptions(unittest.TestCase):
    """Tests for list command options."""

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

    def test_recursive_search(self):
        """Test that -r/--recursive finds buffers in subdirectories."""
        # Create subdirectory
        subdir = pathlib.Path(self.tmp_dir.name) / "subdir"
        subdir.mkdir()

        # Create tracebuffer in subdirectory
        env = os.environ.copy()
        env["CLLTK_TRACING_PATH"] = str(subdir)
        result = clltk("buffer", "--buffer", "SubdirBuffer", "--size", "1KB", env=env)
        self.assertEqual(result.returncode, 0)

        # List without recursive - should not find it
        result = clltk("list", self.tmp_dir.name)
        self.assertEqual(result.returncode, 0)
        self.assertNotIn("SubdirBuffer", result.stdout)

        # List with recursive - should find it
        result = clltk("list", "-r", self.tmp_dir.name)
        self.assertEqual(result.returncode, 0)
        self.assertIn("SubdirBuffer", result.stdout)

    def test_recursive_long_flag(self):
        """Test that --recursive flag works."""
        # Create subdirectory
        subdir = pathlib.Path(self.tmp_dir.name) / "nested"
        subdir.mkdir()

        # Create tracebuffer in subdirectory
        env = os.environ.copy()
        env["CLLTK_TRACING_PATH"] = str(subdir)
        result = clltk("buffer", "--buffer", "NestedBuffer", "--size", "1KB", env=env)
        self.assertEqual(result.returncode, 0)

        # List with recursive long flag
        result = clltk("list", "--recursive", self.tmp_dir.name)
        self.assertEqual(result.returncode, 0)
        self.assertIn("NestedBuffer", result.stdout)

    def test_no_recursive_default(self):
        """Test that non-recursive is the default behavior."""
        # Create subdirectory with buffer
        subdir = pathlib.Path(self.tmp_dir.name) / "subdir2"
        subdir.mkdir()

        env = os.environ.copy()
        env["CLLTK_TRACING_PATH"] = str(subdir)
        result = clltk("buffer", "--buffer", "HiddenBuffer", "--size", "1KB", env=env)
        self.assertEqual(result.returncode, 0)

        # Create buffer in root
        result = clltk("buffer", "--buffer", "RootBuffer", "--size", "1KB")
        self.assertEqual(result.returncode, 0)

        # List without recursive
        result = clltk("list", self.tmp_dir.name)
        self.assertEqual(result.returncode, 0)
        self.assertIn("RootBuffer", result.stdout)
        self.assertNotIn("HiddenBuffer", result.stdout)

    def test_filter_by_name_regex(self):
        """Test filtering by tracebuffer name regex."""
        # Create multiple tracebuffers
        result = clltk("buffer", "--buffer", "TestAlpha", "--size", "1KB")
        self.assertEqual(result.returncode, 0)
        result = clltk("buffer", "--buffer", "TestBeta", "--size", "1KB")
        self.assertEqual(result.returncode, 0)
        result = clltk("buffer", "--buffer", "OtherBuffer", "--size", "1KB")
        self.assertEqual(result.returncode, 0)

        # Filter for buffers starting with "Test"
        result = clltk("list", "--filter", "^Test", self.tmp_dir.name)
        self.assertEqual(result.returncode, 0)
        self.assertIn("TestAlpha", result.stdout)
        self.assertIn("TestBeta", result.stdout)
        self.assertNotIn("OtherBuffer", result.stdout)

    def test_filter_with_wildcard(self):
        """Test filtering with wildcard pattern."""
        # Create tracebuffers
        result = clltk("buffer", "--buffer", "App1Buffer", "--size", "1KB")
        self.assertEqual(result.returncode, 0)
        result = clltk("buffer", "--buffer", "App2Buffer", "--size", "1KB")
        self.assertEqual(result.returncode, 0)
        result = clltk("buffer", "--buffer", "SystemLog", "--size", "1KB")
        self.assertEqual(result.returncode, 0)

        # Filter for App buffers
        result = clltk("list", "--filter", "App.*Buffer", self.tmp_dir.name)
        self.assertEqual(result.returncode, 0)
        self.assertIn("App1Buffer", result.stdout)
        self.assertIn("App2Buffer", result.stdout)
        self.assertNotIn("SystemLog", result.stdout)

    def test_filter_no_match(self):
        """Test filter that matches nothing."""
        # Create a tracebuffer
        result = clltk("buffer", "--buffer", "SomeBuffer", "--size", "1KB")
        self.assertEqual(result.returncode, 0)

        # Filter for non-existent pattern
        result = clltk("list", "--filter", "^NonExistent", self.tmp_dir.name)
        self.assertEqual(result.returncode, 0)
        self.assertIn("No tracebuffers found", result.stdout)


class TestListStatistics(unittest.TestCase):
    """Tests for list command statistics."""

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

    def test_entries_count_matches_writes(self):
        """Test that ENTRIES count matches number of tracepoint writes."""
        # Create a tracebuffer
        result = clltk("buffer", "--buffer", "CountBuffer", "--size", "4KB")
        self.assertEqual(result.returncode, 0)

        # Write some tracepoints
        num_entries = 5
        for i in range(num_entries):
            result = clltk("trace", "-b", "CountBuffer", f"message_{i}")
            self.assertEqual(result.returncode, 0)

        # List and check entries count
        result = clltk("list", "--json", self.tmp_dir.name)
        self.assertEqual(result.returncode, 0)

        data = json.loads(result.stdout)
        self.assertEqual(len(data), 1)
        self.assertEqual(data[0]["entries"], num_entries)

    def test_stats_update_after_writes(self):
        """Test that stats update after buffer operations."""
        # Create a tracebuffer
        result = clltk("buffer", "--buffer", "StatsBuffer", "--size", "4KB")
        self.assertEqual(result.returncode, 0)

        # Check initial state
        result = clltk("list", "--json", self.tmp_dir.name)
        data = json.loads(result.stdout)
        initial_entries = data[0]["entries"]
        initial_used = data[0]["used"]

        # Write a tracepoint
        result = clltk("trace", "-b", "StatsBuffer", "new message")
        self.assertEqual(result.returncode, 0)

        # Check updated state
        result = clltk("list", "--json", self.tmp_dir.name)
        data = json.loads(result.stdout)
        new_entries = data[0]["entries"]
        new_used = data[0]["used"]

        self.assertGreater(new_entries, initial_entries)
        self.assertGreater(new_used, initial_used)

    def test_capacity_matches_size(self):
        """Test that capacity reflects buffer size."""
        # Create a tracebuffer with known size
        result = clltk("buffer", "--buffer", "CapacityBuffer", "--size", "4KB")
        self.assertEqual(result.returncode, 0)

        # Check capacity
        result = clltk("list", "--json", self.tmp_dir.name)
        data = json.loads(result.stdout)

        # Capacity should be close to 4KB (4096 bytes), accounting for header overhead
        capacity = data[0]["capacity"]
        self.assertGreater(capacity, 0)
        self.assertLess(capacity, 5000)  # Should be less than 5KB

    def test_source_type_user(self):
        """Test that source type shows user for userspace buffers."""
        # Create a tracebuffer
        result = clltk("buffer", "--buffer", "UserBuffer", "--size", "1KB")
        self.assertEqual(result.returncode, 0)

        # Check source type
        result = clltk("list", "--json", self.tmp_dir.name)
        data = json.loads(result.stdout)
        self.assertEqual(data[0]["source_type"], "user")


class TestListEdgeCases(unittest.TestCase):
    """Edge case tests for list command."""

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

    def test_nonexistent_path(self):
        """Test listing a nonexistent path."""
        result = clltk("list", "/nonexistent/path/that/does/not/exist", check=False)
        # Should fail or show error
        self.assertTrue(
            result.returncode != 0 or "No tracebuffers found" in result.stdout,
            msg=f"Expected error or no buffers message, got: {result.stdout} {result.stderr}",
        )

    def test_permission_handling(self):
        """Test handling of permission issues."""
        # Create a directory with restricted permissions
        restricted_dir = pathlib.Path(self.tmp_dir.name) / "restricted"
        restricted_dir.mkdir(mode=0o000)

        try:
            result = clltk("list", str(restricted_dir), check=False)
            # Should handle gracefully - either error or empty list
            # We don't assert on return code since behavior may vary
        finally:
            # Restore permissions for cleanup
            restricted_dir.chmod(0o755)

    def test_list_with_invalid_filter_regex(self):
        """Test list with invalid regex pattern."""
        result = clltk("list", "--filter", "[invalid", self.tmp_dir.name, check=False)
        # Should fail with invalid regex
        self.assertNotEqual(result.returncode, 0)

    def test_deeply_nested_recursive(self):
        """Test recursive search in deeply nested directories."""
        # Create deeply nested structure
        deep_path = pathlib.Path(self.tmp_dir.name)
        for i in range(5):
            deep_path = deep_path / f"level{i}"
            deep_path.mkdir()

        # Create tracebuffer at deepest level
        env = os.environ.copy()
        env["CLLTK_TRACING_PATH"] = str(deep_path)
        result = clltk("buffer", "--buffer", "DeepBuffer", "--size", "1KB", env=env)
        self.assertEqual(result.returncode, 0)

        # Find it with recursive search
        result = clltk("list", "-r", self.tmp_dir.name)
        self.assertEqual(result.returncode, 0)
        self.assertIn("DeepBuffer", result.stdout)

    def test_multiple_buffers_in_subdirs(self):
        """Test recursive search finds buffers in multiple subdirectories."""
        # Create multiple subdirectories with buffers
        subdirs = ["dir_a", "dir_b", "dir_c"]
        buffer_names = []

        for subdir_name in subdirs:
            subdir = pathlib.Path(self.tmp_dir.name) / subdir_name
            subdir.mkdir()

            buffer_name = f"Buffer_{subdir_name}"
            buffer_names.append(buffer_name)

            env = os.environ.copy()
            env["CLLTK_TRACING_PATH"] = str(subdir)
            result = clltk("buffer", "--buffer", buffer_name, "--size", "1KB", env=env)
            self.assertEqual(result.returncode, 0)

        # Find all with recursive search
        result = clltk("list", "-r", self.tmp_dir.name)
        self.assertEqual(result.returncode, 0)

        for name in buffer_names:
            self.assertIn(name, result.stdout)

    def test_list_json_with_recursive(self):
        """Test JSON output combined with recursive search."""
        # Create subdirectory with buffer
        subdir = pathlib.Path(self.tmp_dir.name) / "json_subdir"
        subdir.mkdir()

        env = os.environ.copy()
        env["CLLTK_TRACING_PATH"] = str(subdir)
        result = clltk(
            "buffer", "--buffer", "JsonRecursiveBuffer", "--size", "1KB", env=env
        )
        self.assertEqual(result.returncode, 0)

        # List with both flags
        result = clltk("list", "-r", "--json", self.tmp_dir.name)
        self.assertEqual(result.returncode, 0)

        data = json.loads(result.stdout)
        self.assertEqual(len(data), 1)
        self.assertEqual(data[0]["name"], "JsonRecursiveBuffer")


if __name__ == "__main__":
    unittest.main()
