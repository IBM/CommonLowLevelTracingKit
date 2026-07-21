#!/usr/bin/python3
# Copyright (c) 2024, International Business Machines
# SPDX-License-Identifier: BSD-2-Clause-Patent

"""Level 2+: Test CLLTK consumed from a downstream SHARED library.

The regression this guards: a tracepoint compiled *into a shared object* emits
the _clltk_<buffer>_metaptr metadata section. If that section is read-only, its
absolute pointers become text relocations (DT_TEXTREL) - which -fPIC cannot
avoid, which hardened/PIE targets reject at load time, and which a packaging QA
gate (e.g. Yocto do_package_qa) fails outright.

The other consumer tests build an *executable*, where the linker resolves those
pointers at link time and no text relocation appears - so they never exercise
this shape. This test builds a genuine consumer shared library against the
*installed* package and inspects the produced .so with readelf, exactly as a
hardened packaging QA would. It is deliberately built with no special link
flags: the .so must be clean the way an ordinary consumer builds it.
"""

import os
import pathlib
import shutil
import subprocess
import tempfile
import unittest

from .helpers.consumer import (
    build_cmake_project,
    configure_cmake_project,
    create_temp_project_copy,
)
from .helpers.rpm import (
    cmake_install_to_prefix,
    ensure_rpms_built,
)

FIXTURE_DIR = pathlib.Path(__file__).parent / "consumer_shared_project"


def setUpModule():
    """Ensure RPM packages are built before running any tests."""
    ensure_rpms_built()


def _find_plugin_so(project_dir: pathlib.Path) -> pathlib.Path:
    """Locate the built consumer plugin shared object."""
    matches = list((project_dir / "build").rglob("libconsumer_plugin*.so"))
    return matches[0] if matches else None


@unittest.skipUnless(shutil.which("readelf"), "readelf (binutils) not available")
class TestConsumerSharedLib(unittest.TestCase):
    """A downstream shared library that traces must be text-relocation free."""

    _prefix = None
    _project_dir = None

    @classmethod
    def setUpClass(cls):
        cls._prefix = pathlib.Path(tempfile.mkdtemp(prefix="clltk_sharedlib_prefix_"))
        cmake_install_to_prefix(cls._prefix)
        cls._project_dir = create_temp_project_copy(FIXTURE_DIR)

    @classmethod
    def tearDownClass(cls):
        if cls._prefix and cls._prefix.exists():
            shutil.rmtree(cls._prefix, ignore_errors=True)
        if cls._project_dir and cls._project_dir.exists():
            shutil.rmtree(cls._project_dir.parent, ignore_errors=True)

    def _ensure_built(self):
        cfg = configure_cmake_project(self._project_dir, self._prefix)
        self.assertTrue(
            cfg.success,
            f"CMake configure failed:\nstdout: {cfg.stdout}\nstderr: {cfg.stderr}",
        )
        build = build_cmake_project(self._project_dir)
        self.assertTrue(
            build.success,
            f"CMake build failed:\nstdout: {build.stdout}\nstderr: {build.stderr}",
        )

    def test_01_build(self):
        """The consumer shared library and its driver must build."""
        self._ensure_built()
        so = _find_plugin_so(self._project_dir)
        self.assertIsNotNone(so, "libconsumer_plugin.so was not produced")

    def test_02_no_text_relocations(self):
        """The consumer .so must have no DT_TEXTREL (the do_package_qa check)."""
        self._ensure_built()
        so = _find_plugin_so(self._project_dir)
        self.assertIsNotNone(so, "libconsumer_plugin.so was not produced")

        dyn = subprocess.run(
            ["readelf", "-dW", str(so)],
            capture_output=True,
            text=True,
        )
        self.assertEqual(dyn.returncode, 0, f"readelf -d failed: {dyn.stderr}")
        offending = [ln for ln in dyn.stdout.splitlines() if "TEXTREL" in ln]
        self.assertEqual(
            offending,
            [],
            "consumer shared library has text relocations (DT_TEXTREL); a "
            "tracepoint's _metaptr section regressed to read-only. This breaks "
            "PIE/hardened targets and packaging QA. Offending readelf lines:\n"
            + "\n".join(offending),
        )

    def test_03_no_executable_stack(self):
        """The consumer .so must not request an executable stack."""
        self._ensure_built()
        so = _find_plugin_so(self._project_dir)
        self.assertIsNotNone(so, "libconsumer_plugin.so was not produced")

        seg = subprocess.run(
            ["readelf", "-lW", str(so)],
            capture_output=True,
            text=True,
        )
        self.assertEqual(seg.returncode, 0, f"readelf -l failed: {seg.stderr}")
        for line in seg.stdout.splitlines():
            if "GNU_STACK" in line:
                # Flags column: RWE - an 'E' means executable stack.
                self.assertNotIn(
                    "RWE",
                    line,
                    f"consumer shared library requests an executable stack: {line}",
                )

    def test_04_run_traces_across_so(self):
        """Driving the plugin must produce a trace file (cross-.so tracing)."""
        self._ensure_built()
        build_dir = self._project_dir / "build"
        app = build_dir / "app"
        self.assertTrue(app.exists(), "driver executable 'app' was not built")

        env = os.environ.copy()
        trace_dir = self._project_dir / "traces"
        trace_dir.mkdir(exist_ok=True)
        env["CLLTK_TRACING_PATH"] = str(trace_dir)
        lib_dirs = [
            str(build_dir),
            str(self._prefix / "lib64"),
            str(self._prefix / "lib"),
        ]
        existing = env.get("LD_LIBRARY_PATH", "")
        env["LD_LIBRARY_PATH"] = ":".join(lib_dirs + ([existing] if existing else []))

        run = subprocess.run(
            [str(app)], cwd=build_dir, env=env, capture_output=True, text=True
        )
        self.assertEqual(
            run.returncode,
            0,
            f"driver failed:\nstdout: {run.stdout}\nstderr: {run.stderr}",
        )
        traces = list(trace_dir.glob("*.clltk_trace"))
        self.assertTrue(len(traces) > 0, f"no trace files produced in {trace_dir}")


if __name__ == "__main__":
    unittest.main()
