#!/usr/bin/python3
# Copyright (c) 2024, International Business Machines
# SPDX-License-Identifier: BSD-2-Clause-Patent

"""Level 1: Validate RPM package artifacts (metadata, file lists, dependencies, scriptlets).

These tests inspect the produced RPM files without installing them.
They require rpmbuild to have been run first (cmake --workflow --preset rpms).
"""

import unittest

from .helpers.rpm import (
    ensure_rpms_built,
    find_rpm_by_name,
    find_srpms,
    get_version_string,
    packages_exist,
    rpm_inspect,
)

VERSION = get_version_string()


def setUpModule():
    """Ensure RPM packages are built before running any tests."""
    ensure_rpms_built()


class TestTracingRpm(unittest.TestCase):
    """Validate the clltk-tracing runtime RPM."""

    @classmethod
    def setUpClass(cls):
        rpm_path = find_rpm_by_name(r"clltk-tracing-\d")
        if rpm_path is None:
            raise unittest.SkipTest("clltk-tracing RPM not found")
        cls.rpm = rpm_inspect(rpm_path)

    def test_name(self):
        self.assertEqual(self.rpm.info.name, "clltk-tracing")

    def test_version(self):
        self.assertEqual(self.rpm.info.version, VERSION)

    def test_license(self):
        self.assertIn("BSD", self.rpm.info.license)

    def test_contains_shared_lib(self):
        so_files = [f for f in self.rpm.files if "libclltk_tracing.so." in f]
        self.assertTrue(len(so_files) > 0, "Missing versioned shared library")

    def test_contains_soversion_symlink(self):
        so1 = [f for f in self.rpm.files if f.endswith("libclltk_tracing.so.1")]
        self.assertTrue(len(so1) > 0, "Missing soversion symlink")

    def test_no_headers(self):
        headers = [f for f in self.rpm.files if f.endswith(".h")]
        self.assertEqual(
            len(headers), 0, f"Runtime RPM should not contain headers: {headers}"
        )

    def test_no_static_libs(self):
        archives = [f for f in self.rpm.files if f.endswith(".a")]
        self.assertEqual(
            len(archives), 0, f"Runtime RPM should not contain static libs: {archives}"
        )

    def test_no_unversioned_so(self):
        """The unversioned .so symlink belongs in devel, not runtime."""
        unversioned = [f for f in self.rpm.files if f.endswith("libclltk_tracing.so")]
        self.assertEqual(len(unversioned), 0, "Unversioned .so should be in devel RPM")

    def test_ldconfig_scriptlet(self):
        scripts = self.rpm.scripts
        has_ldconfig = any("ldconfig" in v for v in scripts.values())
        self.assertTrue(has_ldconfig, f"Missing ldconfig scriptlet. Scripts: {scripts}")


class TestDecoderLibsRpm(unittest.TestCase):
    """Validate the clltk-decoder runtime RPM."""

    @classmethod
    def setUpClass(cls):
        rpm_path = find_rpm_by_name(r"clltk-decoder-\d")
        if rpm_path is None:
            raise unittest.SkipTest("clltk-decoder RPM not found")
        cls.rpm = rpm_inspect(rpm_path)

    def test_name(self):
        self.assertEqual(self.rpm.info.name, "clltk-decoder")

    def test_version(self):
        self.assertEqual(self.rpm.info.version, VERSION)

    def test_contains_shared_lib(self):
        so_files = [f for f in self.rpm.files if "libclltk_decoder.so." in f]
        self.assertTrue(len(so_files) > 0, "Missing versioned shared library")

    def test_no_headers(self):
        headers = [f for f in self.rpm.files if f.endswith((".h", ".hpp"))]
        self.assertEqual(
            len(headers), 0, f"Runtime RPM should not contain headers: {headers}"
        )

    def test_no_static_libs(self):
        archives = [f for f in self.rpm.files if f.endswith(".a")]
        self.assertEqual(len(archives), 0, "Runtime RPM should not contain static libs")

    def test_no_unversioned_so(self):
        """The unversioned .so symlink belongs in devel, not runtime."""
        unversioned = [f for f in self.rpm.files if f.endswith("libclltk_decoder.so")]
        self.assertEqual(len(unversioned), 0, "Unversioned .so should be in devel RPM")

    def test_ldconfig_scriptlet(self):
        scripts = self.rpm.scripts
        has_ldconfig = any("ldconfig" in v for v in scripts.values())
        self.assertTrue(has_ldconfig, "Missing ldconfig scriptlet")


class TestSnapshotRpm(unittest.TestCase):
    """Validate the clltk-snapshot runtime RPM."""

    @classmethod
    def setUpClass(cls):
        rpm_path = find_rpm_by_name(r"clltk-snapshot-\d")
        if rpm_path is None:
            raise unittest.SkipTest("clltk-snapshot RPM not found")
        cls.rpm = rpm_inspect(rpm_path)

    def test_name(self):
        self.assertEqual(self.rpm.info.name, "clltk-snapshot")

    def test_version(self):
        self.assertEqual(self.rpm.info.version, VERSION)

    def test_contains_shared_lib(self):
        so_files = [f for f in self.rpm.files if "libclltk_snapshot.so." in f]
        self.assertTrue(len(so_files) > 0, "Missing versioned shared library")

    def test_no_headers(self):
        headers = [f for f in self.rpm.files if f.endswith((".h", ".hpp"))]
        self.assertEqual(len(headers), 0, "Runtime RPM should not contain headers")

    def test_no_static_libs(self):
        archives = [f for f in self.rpm.files if f.endswith(".a")]
        self.assertEqual(len(archives), 0, "Runtime RPM should not contain static libs")

    def test_no_unversioned_so(self):
        """The unversioned .so symlink belongs in devel, not runtime."""
        unversioned = [f for f in self.rpm.files if f.endswith("libclltk_snapshot.so")]
        self.assertEqual(len(unversioned), 0, "Unversioned .so should be in devel RPM")

    def test_ldconfig_scriptlet(self):
        scripts = self.rpm.scripts
        has_ldconfig = any("ldconfig" in v for v in scripts.values())
        self.assertTrue(has_ldconfig, "Missing ldconfig scriptlet")


class TestDevelRpm(unittest.TestCase):
    """Validate the clltk-devel RPM."""

    @classmethod
    def setUpClass(cls):
        rpm_path = find_rpm_by_name(r"clltk-devel-\d")
        if rpm_path is None:
            raise unittest.SkipTest("clltk-devel RPM not found")
        cls.rpm = rpm_inspect(rpm_path)

    def test_name(self):
        self.assertEqual(self.rpm.info.name, "clltk-devel")

    def test_version(self):
        self.assertEqual(self.rpm.info.version, VERSION)

    def test_contains_tracing_headers(self):
        headers = [f for f in self.rpm.files if "/tracing/tracing.h" in f]
        self.assertTrue(len(headers) > 0, "Missing tracing headers")

    def test_contains_decoder_headers(self):
        headers = [f for f in self.rpm.files if "/decoder/" in f and f.endswith(".hpp")]
        self.assertTrue(len(headers) > 0, "Missing decoder headers")

    def test_contains_snapshot_headers(self):
        headers = [f for f in self.rpm.files if "/snapshot/snapshot.hpp" in f]
        self.assertTrue(len(headers) > 0, "Missing snapshot header")

    def test_contains_cmake_config(self):
        cmake_files = [f for f in self.rpm.files if "CLLTKConfig.cmake" in f]
        self.assertTrue(len(cmake_files) > 0, "Missing CLLTKConfig.cmake")

    def test_contains_cmake_version(self):
        version_files = [f for f in self.rpm.files if "CLLTKConfigVersion.cmake" in f]
        self.assertTrue(len(version_files) > 0, "Missing CLLTKConfigVersion.cmake")

    def test_contains_cmake_targets(self):
        targets_files = [f for f in self.rpm.files if "CLLTKTracingTargets.cmake" in f]
        self.assertTrue(len(targets_files) > 0, "Missing CLLTKTracingTargets.cmake")

    def test_contains_pkgconfig_tracing(self):
        pc_files = [f for f in self.rpm.files if "clltk_tracing.pc" in f]
        self.assertTrue(len(pc_files) > 0, "Missing clltk_tracing.pc")

    def test_contains_pkgconfig_decoder(self):
        pc_files = [f for f in self.rpm.files if "clltk_decoder.pc" in f]
        self.assertTrue(len(pc_files) > 0, "Missing clltk_decoder.pc")

    def test_contains_pkgconfig_snapshot(self):
        pc_files = [f for f in self.rpm.files if "clltk_snapshot.pc" in f]
        self.assertTrue(len(pc_files) > 0, "Missing clltk_snapshot.pc")

    def test_requires_tracing(self):
        tracing_dep = [r for r in self.rpm.requires if "clltk-tracing" in r]
        self.assertTrue(
            len(tracing_dep) > 0,
            f"Missing dep on clltk-tracing. Requires: {self.rpm.requires}",
        )

    def test_requires_decoder(self):
        decoder_dep = [r for r in self.rpm.requires if "clltk-decoder" in r]
        self.assertTrue(
            len(decoder_dep) > 0,
            f"Missing dep on clltk-decoder. Requires: {self.rpm.requires}",
        )

    def test_requires_snapshot(self):
        snapshot_dep = [r for r in self.rpm.requires if "clltk-snapshot" in r]
        self.assertTrue(
            len(snapshot_dep) > 0,
            f"Missing dep on clltk-snapshot. Requires: {self.rpm.requires}",
        )

    def test_contains_unversioned_so_symlinks(self):
        """Devel RPM should contain unversioned .so symlinks for all libs."""
        for lib in [
            "libclltk_tracing.so",
            "libclltk_decoder.so",
            "libclltk_snapshot.so",
        ]:
            with self.subTest(lib=lib):
                matches = [f for f in self.rpm.files if f.endswith(lib)]
                self.assertTrue(
                    len(matches) > 0,
                    f"Missing unversioned symlink {lib} in devel RPM",
                )


class TestStaticRpm(unittest.TestCase):
    """Validate the clltk-static RPM."""

    @classmethod
    def setUpClass(cls):
        rpm_path = find_rpm_by_name(r"clltk-static-\d")
        if rpm_path is None:
            raise unittest.SkipTest("clltk-static RPM not found")
        cls.rpm = rpm_inspect(rpm_path)

    def test_name(self):
        self.assertEqual(self.rpm.info.name, "clltk-static")

    def test_contains_tracing_static(self):
        archives = [f for f in self.rpm.files if "libclltk_tracing_static.a" in f]
        self.assertTrue(len(archives) > 0, "Missing tracing static library")

    def test_contains_decoder_static(self):
        archives = [f for f in self.rpm.files if "libclltk_decoder_static.a" in f]
        self.assertTrue(len(archives) > 0, "Missing decoder static library")

    def test_contains_snapshot_static(self):
        archives = [f for f in self.rpm.files if "libclltk_snapshot_static.a" in f]
        self.assertTrue(len(archives) > 0, "Missing snapshot static library")

    def test_no_shared_libs(self):
        shared = [f for f in self.rpm.files if ".so" in f]
        self.assertEqual(
            len(shared), 0, f"Static RPM should not contain shared libs: {shared}"
        )

    def test_requires_devel(self):
        devel_dep = [r for r in self.rpm.requires if "clltk-devel" in r]
        self.assertTrue(
            len(devel_dep) > 0,
            f"Missing dep on clltk-devel. Requires: {self.rpm.requires}",
        )


class TestToolsRpm(unittest.TestCase):
    """Validate the clltk-tools RPM."""

    @classmethod
    def setUpClass(cls):
        rpm_path = find_rpm_by_name(r"clltk-tools-\d")
        if rpm_path is None:
            raise unittest.SkipTest("clltk-tools RPM not found")
        cls.rpm = rpm_inspect(rpm_path)

    def test_name(self):
        self.assertEqual(self.rpm.info.name, "clltk-tools")

    def test_version(self):
        self.assertEqual(self.rpm.info.version, VERSION)

    def test_contains_clltk_binary(self):
        binaries = [f for f in self.rpm.files if f.endswith("/clltk")]
        self.assertTrue(len(binaries) > 0, "Missing clltk binary")

    def test_no_manual_tracing_dep(self):
        """clltk binary statically links tracing; RPM auto-requires handles shared lib deps."""
        manual_dep = [r for r in self.rpm.requires if "clltk-tracing" in r and "=" in r]
        self.assertEqual(
            len(manual_dep),
            0,
            f"Should not have manual dep on clltk-tracing: {manual_dep}",
        )


class TestPythonDecoderRpm(unittest.TestCase):
    """Validate the clltk-python-decoder RPM."""

    @classmethod
    def setUpClass(cls):
        rpm_path = find_rpm_by_name(r"clltk-python-decoder-\d")
        if rpm_path is None:
            raise unittest.SkipTest("clltk-python-decoder RPM not found")
        cls.rpm = rpm_inspect(rpm_path)

    def test_name(self):
        self.assertEqual(self.rpm.info.name, "clltk-python-decoder")

    def test_version(self):
        self.assertEqual(self.rpm.info.version, VERSION)

    def test_contains_decoder_script(self):
        scripts = [f for f in self.rpm.files if "clltk_decoder.py" in f]
        self.assertTrue(len(scripts) > 0, "Missing clltk_decoder.py")

    def test_is_noarch(self):
        self.assertEqual(self.rpm.info.architecture, "noarch")


class TestFullRpm(unittest.TestCase):
    """Validate the monolithic full RPM."""

    @classmethod
    def setUpClass(cls):
        # The full RPM uses the base package name without a component suffix
        rpm_path = find_rpm_by_name(r"^clltk-\d")
        if rpm_path is None:
            raise unittest.SkipTest("clltk full RPM not found")
        cls.rpm = rpm_inspect(rpm_path)

    def test_version(self):
        self.assertEqual(self.rpm.info.version, VERSION)

    def test_contains_shared_libs(self):
        so_files = [f for f in self.rpm.files if ".so." in f]
        self.assertTrue(
            len(so_files) >= 3, f"Expected at least 3 shared libs, got: {so_files}"
        )

    def test_contains_headers(self):
        headers = [f for f in self.rpm.files if f.endswith((".h", ".hpp"))]
        self.assertTrue(len(headers) > 0, "Full RPM should contain headers")

    def test_contains_clltk_binary(self):
        binaries = [f for f in self.rpm.files if f.endswith("/clltk")]
        self.assertTrue(len(binaries) > 0, "Full RPM should contain clltk binary")

    def test_contains_cmake_config(self):
        cmake_files = [f for f in self.rpm.files if "CLLTKConfig.cmake" in f]
        self.assertTrue(len(cmake_files) > 0, "Full RPM should contain CMake config")

    def test_contains_pkgconfig(self):
        pc_files = [f for f in self.rpm.files if f.endswith(".pc")]
        self.assertTrue(
            len(pc_files) >= 3, f"Expected at least 3 .pc files, got: {pc_files}"
        )


class TestDebuginfoRpms(unittest.TestCase):
    """Validate that debuginfo RPMs are produced for compiled packages."""

    def _find_debuginfo(self, name_pattern):
        """Find a debuginfo RPM matching the given name pattern."""
        return find_rpm_by_name(name_pattern)

    def test_tracing_debuginfo(self):
        rpm = self._find_debuginfo(r"clltk-tracing-debuginfo-\d")
        self.assertIsNotNone(rpm, "Missing clltk-tracing-debuginfo RPM")

    def test_decoder_debuginfo(self):
        rpm = self._find_debuginfo(r"clltk-decoder-debuginfo-\d")
        self.assertIsNotNone(rpm, "Missing clltk-decoder-debuginfo RPM")

    def test_snapshot_debuginfo(self):
        rpm = self._find_debuginfo(r"clltk-snapshot-debuginfo-\d")
        self.assertIsNotNone(rpm, "Missing clltk-snapshot-debuginfo RPM")

    def test_tools_debuginfo(self):
        rpm = self._find_debuginfo(r"clltk-tools-debuginfo-\d")
        self.assertIsNotNone(rpm, "Missing clltk-tools-debuginfo RPM")


class TestSrpm(unittest.TestCase):
    """Validate the source RPM."""

    @classmethod
    def setUpClass(cls):
        srpms = find_srpms()
        if not srpms:
            raise unittest.SkipTest("No SRPM found")
        cls.srpm_path = srpms[0]
        cls.files = []
        # rpm -qpl on SRPMs lists the source files
        import subprocess

        result = subprocess.run(
            ["rpm", "-qpl", str(cls.srpm_path)], capture_output=True, text=True
        )
        if result.returncode == 0:
            cls.files = result.stdout.strip().splitlines()

    def test_srpm_exists(self):
        self.assertTrue(self.srpm_path.exists())

    def test_srpm_filename(self):
        self.assertIn(".src.rpm", self.srpm_path.name)

    def test_contains_source(self):
        """SRPM should contain a source tarball or source files."""
        self.assertTrue(len(self.files) > 0, "SRPM contains no files")


if __name__ == "__main__":
    unittest.main()
