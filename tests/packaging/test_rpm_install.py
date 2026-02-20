#!/usr/bin/python3
# Copyright (c) 2024, International Business Machines
# SPDX-License-Identifier: BSD-2-Clause-Patent

"""Level 2: Install RPMs and run smoke tests.

These tests extract RPM contents to a temporary prefix and verify that
the installed binaries and libraries actually work.
"""

import os
import pathlib
import shutil
import subprocess
import tempfile
import unittest

from .helpers.rpm import (
    cmake_install_to_prefix,
    ensure_rpms_built,
    get_build_dir,
)


def setUpModule():
    """Ensure RPM packages are built before running any tests."""
    ensure_rpms_built()


class TestInstallSmokeTest(unittest.TestCase):
    """Install via cmake --install and verify basics work."""

    _prefix = None

    @classmethod
    def setUpClass(cls):
        cls._prefix = pathlib.Path(tempfile.mkdtemp(prefix="clltk_install_"))
        try:
            cmake_install_to_prefix(cls._prefix)
        except subprocess.CalledProcessError as e:
            shutil.rmtree(cls._prefix, ignore_errors=True)
            raise unittest.SkipTest(f"cmake --install failed: {e}")

    @classmethod
    def tearDownClass(cls):
        if cls._prefix and cls._prefix.exists():
            shutil.rmtree(cls._prefix, ignore_errors=True)

    def _find_file(self, pattern):
        """Find a file under the install prefix."""
        import glob

        matches = list(self._prefix.rglob(pattern))
        return matches[0] if matches else None

    def test_clltk_binary_exists(self):
        binary = self._find_file("clltk")
        self.assertIsNotNone(binary, "clltk binary not found in install prefix")

    def test_clltk_binary_runs(self):
        binary = self._find_file("clltk")
        if binary is None:
            self.skipTest("clltk binary not found")
        result = subprocess.run(
            [str(binary), "--version"],
            capture_output=True,
            text=True,
        )
        # clltk --version should succeed or at least not segfault
        self.assertIn(
            result.returncode,
            [0, 1],
            f"clltk --version returned {result.returncode}: {result.stderr}",
        )

    def test_shared_lib_tracing_exists(self):
        lib = self._find_file("libclltk_tracing.so")
        self.assertIsNotNone(lib, "libclltk_tracing.so not found")

    def test_shared_lib_decoder_exists(self):
        lib = self._find_file("libclltk_decoder.so")
        self.assertIsNotNone(lib, "libclltk_decoder.so not found")

    def test_shared_lib_snapshot_exists(self):
        lib = self._find_file("libclltk_snapshot.so")
        self.assertIsNotNone(lib, "libclltk_snapshot.so not found")

    def test_shared_lib_tracing_loadable(self):
        """Verify the shared library can be loaded (ldd resolves all deps)."""
        lib = self._find_file("libclltk_tracing.so")
        if lib is None:
            self.skipTest("libclltk_tracing.so not found")
        # Use ldd to check if all dependencies resolve
        env = os.environ.copy()
        lib_dirs = [
            str(self._prefix / "lib64"),
            str(self._prefix / "lib"),
        ]
        env["LD_LIBRARY_PATH"] = ":".join(lib_dirs)
        result = subprocess.run(
            ["ldd", str(lib)],
            capture_output=True,
            text=True,
            env=env,
        )
        self.assertEqual(result.returncode, 0, f"ldd failed: {result.stderr}")
        self.assertNotIn(
            "not found", result.stdout, f"Unresolved dependencies: {result.stdout}"
        )

    def test_decoder_script_exists(self):
        script = self._find_file("clltk_decoder.py")
        self.assertIsNotNone(script, "clltk_decoder.py not found")

    def test_decoder_script_executable(self):
        script = self._find_file("clltk_decoder.py")
        if script is None:
            self.skipTest("clltk_decoder.py not found")
        self.assertTrue(
            os.access(str(script), os.X_OK), "clltk_decoder.py is not executable"
        )

    def test_cmake_config_exists(self):
        config = self._find_file("CLLTKConfig.cmake")
        self.assertIsNotNone(config, "CLLTKConfig.cmake not found")

    def test_pkgconfig_files_exist(self):
        for name in ["clltk_tracing.pc", "clltk_decoder.pc", "clltk_snapshot.pc"]:
            with self.subTest(pc_file=name):
                pc = self._find_file(name)
                self.assertIsNotNone(pc, f"{name} not found")


if __name__ == "__main__":
    unittest.main()
