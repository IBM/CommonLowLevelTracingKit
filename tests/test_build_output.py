#!/usr/bin/python3
# Copyright (c) 2024, International Business Machines
# SPDX-License-Identifier: BSD-2-Clause-Patent

"""
Build output validation tests.

These tests verify that the built libraries have correct properties:
- Static libraries should contain relocatable object files
- Shared libraries should be Position Independent Code (PIC)

Migrated from: tests/robot.tests/build_output/build_output.robot
"""

import unittest
import os
import sys

# Add tests directory to path for imports
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.realpath(__file__))))

from helpers.base import run_command, get_build_dir
from helpers.library_validation import is_static_lib_relocatable, is_shared_lib_pic


def build_target(target: str) -> None:
    """Build a CMake target."""
    run_command(f"cmake --build --preset default --target {target}")


def setUpModule():
    """Configure CMake before running tests."""
    run_command("cmake --preset default")


class TestBuildOutput(unittest.TestCase):
    """Tests for validating build output library properties."""

    def test_static_tracing_library_is_relocatable(self):
        """Test that static tracing library contains relocatable object files."""
        build_target("clltk_tracing_static")
        
        build_dir = get_build_dir()
        static_lib = build_dir / "tracing_library" / "libclltk_tracing_static.a"
        
        self.assertTrue(
            static_lib.exists(),
            f"static tracing library is missing: {static_lib}"
        )
        self.assertTrue(
            is_static_lib_relocatable(static_lib),
            f"static tracing library is not relocatable: {static_lib}"
        )

    def test_shared_tracing_library_is_pic(self):
        """Test that shared tracing library is Position Independent Code."""
        build_target("clltk_tracing_shared")
        
        build_dir = get_build_dir()
        shared_lib = build_dir / "tracing_library" / "libclltk_tracing.so"
        
        self.assertTrue(
            shared_lib.exists(),
            f"shared tracing library is missing: {shared_lib}"
        )
        self.assertTrue(
            is_shared_lib_pic(shared_lib),
            f"{shared_lib} is not PIC (contains TEXTREL)"
        )

    def test_static_snapshot_library_is_relocatable(self):
        """Test that static snapshot library contains relocatable object files."""
        build_target("clltk_snapshot_static")
        
        build_dir = get_build_dir()
        static_lib = build_dir / "snapshot_library" / "libclltk_snapshot_static.a"
        
        self.assertTrue(
            static_lib.exists(),
            f"static snapshot library is missing: {static_lib}"
        )
        self.assertTrue(
            is_static_lib_relocatable(static_lib),
            f"static snapshot library is not relocatable: {static_lib}"
        )

    def test_shared_snapshot_library_is_pic(self):
        """Test that shared snapshot library is Position Independent Code."""
        build_target("clltk_snapshot_shared")
        
        build_dir = get_build_dir()
        shared_lib = build_dir / "snapshot_library" / "libclltk_snapshot.so"
        
        self.assertTrue(
            shared_lib.exists(),
            f"shared snapshot library is missing: {shared_lib}"
        )
        self.assertTrue(
            is_shared_lib_pic(shared_lib),
            f"{shared_lib} is not PIC (contains TEXTREL)"
        )


if __name__ == '__main__':
    unittest.main()
