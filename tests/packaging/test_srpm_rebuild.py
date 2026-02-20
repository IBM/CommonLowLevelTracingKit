# Copyright (c) 2024, International Business Machines
# SPDX-License-Identifier: BSD-2-Clause-Patent

"""Level 3: Test that the SRPM can be rebuilt from scratch.

This verifies that the source RPM is self-contained and can produce
binary RPMs via rpmbuild --rebuild.

NOTE: CPack's CPACK_SOURCE_GENERATOR=RPM packages the install tree, not the
actual project source. The resulting SRPM cannot be rebuilt with rpmbuild
--rebuild because it lacks CMakeLists.txt, CMakePresets.json, etc. This test
is therefore skipped by default. Enable with CLLTK_TEST_SRPM_REBUILD=1 once
a proper source-based SRPM workflow is implemented.
"""

import os
import pathlib
import shutil
import subprocess
import tempfile
import unittest

from .helpers.rpm import ensure_rpms_built, find_srpms


def setUpModule():
    """Ensure RPM packages are built before running any tests."""
    ensure_rpms_built()


_SRPM_REBUILD_ENABLED = os.environ.get("CLLTK_TEST_SRPM_REBUILD", "0") == "1"


@unittest.skipUnless(shutil.which("rpmbuild"), "rpmbuild not available")
@unittest.skipUnless(
    _SRPM_REBUILD_ENABLED,
    "SRPM rebuild skipped: CPack SRPMs package install tree, not source. "
    "Set CLLTK_TEST_SRPM_REBUILD=1 to enable.",
)
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


if __name__ == "__main__":
    unittest.main()
