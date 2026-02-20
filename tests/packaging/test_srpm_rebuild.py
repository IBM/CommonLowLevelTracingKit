#!/usr/bin/python3
# Copyright (c) 2024, International Business Machines
# SPDX-License-Identifier: BSD-2-Clause-Patent

"""Level 3: Test that the SRPM can be rebuilt from scratch.

This verifies that the source RPM is self-contained and can produce
binary RPMs via rpmbuild --rebuild. The SRPM is built from a real
git archive source tarball with a proper spec file (cmake/clltk.spec.in),
not from CPack's source generator.
"""

import pathlib
import shutil
import subprocess
import tempfile
import unittest

from .helpers.rpm import ensure_rpms_built, find_srpms


def setUpModule():
    """Ensure RPM packages are built before running any tests."""
    ensure_rpms_built()


@unittest.skipUnless(shutil.which("rpmbuild"), "rpmbuild not available")
class TestSrpmRebuild(unittest.TestCase):
    """Test rebuilding binary RPMs from the SRPM."""

    _tmpdir = None

    @classmethod
    def setUpClass(cls):
        srpms = find_srpms()
        if not srpms:
            raise unittest.SkipTest("No SRPM found")
        cls._srpm_path = srpms[0]
        cls._tmpdir = pathlib.Path(tempfile.mkdtemp(prefix="clltk_srpm_rebuild_"))

    @classmethod
    def tearDownClass(cls):
        if cls._tmpdir and cls._tmpdir.exists():
            shutil.rmtree(cls._tmpdir, ignore_errors=True)

    def test_srpm_rebuild(self):
        """rpmbuild --rebuild should produce binary RPMs."""
        result = subprocess.run(
            [
                "rpmbuild",
                "--rebuild",
                str(self._srpm_path),
                "--define",
                f"_topdir {self._tmpdir}",
            ],
            capture_output=True,
            text=True,
            timeout=600,  # 10 minute timeout for a full rebuild
        )
        self.assertEqual(
            result.returncode,
            0,
            f"rpmbuild --rebuild failed (rc={result.returncode}):\n"
            f"stdout: {result.stdout[-2000:]}\n"
            f"stderr: {result.stderr[-2000:]}",
        )

        # Verify binary RPMs were produced
        rpm_dir = self._tmpdir / "RPMS"
        rpms = list(rpm_dir.rglob("*.rpm")) if rpm_dir.exists() else []
        self.assertTrue(
            len(rpms) > 0,
            f"rpmbuild --rebuild produced no RPMs in {rpm_dir}",
        )

        # Verify we got the expected subpackages (at least 7:
        # tracing, decoder, snapshot, devel, static, tools, python-decoder)
        expected_subpackages = [
            "clltk-tracing",
            "clltk-decoder",
            "clltk-snapshot",
            "clltk-devel",
            "clltk-static",
            "clltk-tools",
            "clltk-python-decoder",
        ]
        rpm_names = [p.name for p in rpms]
        for pkg in expected_subpackages:
            found = any(pkg in name for name in rpm_names)
            self.assertTrue(
                found,
                f"Expected subpackage '{pkg}' not found in rebuilt RPMs: {rpm_names}",
            )


if __name__ == "__main__":
    unittest.main()
