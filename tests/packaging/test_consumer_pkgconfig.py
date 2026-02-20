#!/usr/bin/python3
# Copyright (c) 2024, International Business Machines
# SPDX-License-Identifier: BSD-2-Clause-Patent

"""Level 2+: Test CLLTK pkg-config integration.

Installs CLLTK to a temp prefix and verifies that pkg-config returns
correct flags, and that a C file can be compiled using those flags.
"""

import pathlib
import shutil
import subprocess
import tempfile
import unittest

from .helpers.consumer import compile_with_pkg_config, run_pkg_config
from .helpers.rpm import cmake_install_to_prefix, ensure_rpms_built


def setUpModule():
    """Ensure RPM packages are built before running any tests."""
    ensure_rpms_built()


SIMPLE_C_SOURCE = """\
#include <CommonLowLevelTracingKit/tracing/tracing.h>
int main(void) { return 0; }
"""


class TestPkgConfigTracing(unittest.TestCase):
    """Test pkg-config for clltk_tracing."""

    _prefix = None

    @classmethod
    def setUpClass(cls):
        cls._prefix = pathlib.Path(tempfile.mkdtemp(prefix="clltk_pkgconfig_"))
        try:
            cmake_install_to_prefix(cls._prefix)
        except subprocess.CalledProcessError as e:
            shutil.rmtree(cls._prefix, ignore_errors=True)
            raise unittest.SkipTest(f"cmake --install failed: {e}")

    @classmethod
    def tearDownClass(cls):
        if cls._prefix and cls._prefix.exists():
            shutil.rmtree(cls._prefix, ignore_errors=True)

    def test_pkgconfig_tracing_found(self):
        success, cflags, libs = run_pkg_config("clltk_tracing", self._prefix)
        self.assertTrue(success, "pkg-config failed to find clltk_tracing")

    def test_pkgconfig_tracing_cflags(self):
        success, cflags, libs = run_pkg_config("clltk_tracing", self._prefix)
        if not success:
            self.skipTest("pkg-config not available for clltk_tracing")
        self.assertIn("-I", cflags, f"cflags should contain -I: {cflags}")

    def test_pkgconfig_tracing_libs(self):
        success, cflags, libs = run_pkg_config("clltk_tracing", self._prefix)
        if not success:
            self.skipTest("pkg-config not available for clltk_tracing")
        self.assertIn(
            "-lclltk_tracing", libs, f"libs should contain -lclltk_tracing: {libs}"
        )

    def test_pkgconfig_decoder_found(self):
        success, _, _ = run_pkg_config("clltk_decoder", self._prefix)
        self.assertTrue(success, "pkg-config failed to find clltk_decoder")

    def test_pkgconfig_snapshot_found(self):
        success, _, _ = run_pkg_config("clltk_snapshot", self._prefix)
        self.assertTrue(success, "pkg-config failed to find clltk_snapshot")

    @unittest.skipUnless(shutil.which("gcc"), "gcc not available")
    def test_compile_with_pkgconfig(self):
        """Compile a minimal C program using pkg-config flags."""
        result = compile_with_pkg_config(SIMPLE_C_SOURCE, "clltk_tracing", self._prefix)
        self.assertTrue(
            result.success,
            f"Compilation with pkg-config flags failed:\n{result.stderr}",
        )


if __name__ == "__main__":
    unittest.main()
