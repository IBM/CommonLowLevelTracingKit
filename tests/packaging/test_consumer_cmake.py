# Copyright (c) 2024, International Business Machines
# SPDX-License-Identifier: BSD-2-Clause-Patent

"""Level 2+: Test CLLTK as a downstream CMake consumer.

Installs CLLTK to a temp prefix and verifies that a minimal consumer
project can find_package(CLLTK), build, link, and run.
"""

import pathlib
import shutil
import subprocess
import tempfile
import unittest

from .helpers.consumer import (
    build_cmake_project,
    configure_cmake_project,
    create_temp_consumer_project,
    run_consumer_binary,
)
from .helpers.rpm import (
    cmake_install_to_prefix,
    ensure_rpms_built,
)


def setUpModule():
    """Ensure RPM packages are built before running any tests."""
    ensure_rpms_built()


class TestConsumerCMake(unittest.TestCase):
    """Test that a downstream CMake project can use find_package(CLLTK)."""

    _prefix = None
    _project_dir = None

    @classmethod
    def setUpClass(cls):
        # Install CLLTK to temp prefix
        cls._prefix = pathlib.Path(tempfile.mkdtemp(prefix="clltk_cmake_prefix_"))
        try:
            cmake_install_to_prefix(cls._prefix)
        except subprocess.CalledProcessError as e:
            shutil.rmtree(cls._prefix, ignore_errors=True)
            raise unittest.SkipTest(f"cmake --install failed: {e}")

        # Create consumer project
        cls._project_dir = create_temp_consumer_project()

    @classmethod
    def tearDownClass(cls):
        if cls._prefix and cls._prefix.exists():
            shutil.rmtree(cls._prefix, ignore_errors=True)
        if cls._project_dir and cls._project_dir.exists():
            shutil.rmtree(cls._project_dir, ignore_errors=True)

    def test_01_configure(self):
        """CMake configure with find_package(CLLTK) should succeed."""
        result = configure_cmake_project(self._project_dir, self._prefix)
        self.assertTrue(
            result.success,
            f"CMake configure failed:\nstdout: {result.stdout}\nstderr: {result.stderr}",
        )

    def test_02_build(self):
        """Building the consumer project should succeed."""
        # Ensure configured first
        configure_cmake_project(self._project_dir, self._prefix)
        result = build_cmake_project(self._project_dir)
        self.assertTrue(
            result.success,
            f"CMake build failed:\nstdout: {result.stdout}\nstderr: {result.stderr}",
        )

    def test_03_run(self):
        """Running the consumer binary should succeed (exit 0)."""
        configure_cmake_project(self._project_dir, self._prefix)
        build_cmake_project(self._project_dir)
        result = run_consumer_binary(self._project_dir, install_prefix=self._prefix)
        self.assertTrue(
            result.success,
            f"Consumer binary failed:\nstdout: {result.stdout}\nstderr: {result.stderr}",
        )

    def test_04_produces_trace(self):
        """Running the consumer should produce a trace file."""
        configure_cmake_project(self._project_dir, self._prefix)
        build_cmake_project(self._project_dir)
        run_consumer_binary(self._project_dir, install_prefix=self._prefix)
        trace_dir = self._project_dir / "traces"
        trace_files = list(trace_dir.glob("*.clltk_trace"))
        self.assertTrue(
            len(trace_files) > 0,
            f"No trace files produced in {trace_dir}",
        )


if __name__ == "__main__":
    unittest.main()
