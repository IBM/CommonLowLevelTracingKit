# Copyright (c) 2024, International Business Machines
# SPDX-License-Identifier: BSD-2-Clause-Patent

"""Helper utilities for testing CLLTK as a downstream consumer."""

import os
import pathlib
import shutil
import subprocess
import tempfile
from dataclasses import dataclass
from typing import List, Optional, Tuple


CONSUMER_PROJECT_DIR = pathlib.Path(__file__).parent.parent / "consumer_project"


@dataclass
class BuildResult:
    """Result of a build operation."""

    success: bool
    returncode: int
    stdout: str
    stderr: str


def _run(args: List[str], cwd: pathlib.Path, env: Optional[dict] = None) -> BuildResult:
    """Run a command and return structured result."""
    result = subprocess.run(
        args,
        cwd=cwd,
        capture_output=True,
        text=True,
        env=env,
    )
    return BuildResult(
        success=(result.returncode == 0),
        returncode=result.returncode,
        stdout=result.stdout,
        stderr=result.stderr,
    )


def create_temp_consumer_project(
    source_file: str = "main.c",
    cmake_file: str = "CMakeLists.txt",
) -> pathlib.Path:
    """Copy the consumer project to a temporary directory.

    Returns the path to the temporary project directory.
    The caller is responsible for cleaning up (use shutil.rmtree).
    """
    tmpdir = pathlib.Path(tempfile.mkdtemp(prefix="clltk_consumer_"))
    # Copy project files
    shutil.copy2(CONSUMER_PROJECT_DIR / cmake_file, tmpdir / "CMakeLists.txt")
    shutil.copy2(CONSUMER_PROJECT_DIR / source_file, tmpdir / source_file)
    return tmpdir


def configure_cmake_project(
    project_dir: pathlib.Path,
    install_prefix: pathlib.Path,
) -> BuildResult:
    """Configure a CMake consumer project pointing at the CLLTK install prefix."""
    build_dir = project_dir / "build"
    build_dir.mkdir(exist_ok=True)
    return _run(
        [
            "cmake",
            "-S",
            str(project_dir),
            "-B",
            str(build_dir),
            f"-DCMAKE_PREFIX_PATH={install_prefix}",
        ],
        cwd=project_dir,
    )


def build_cmake_project(project_dir: pathlib.Path) -> BuildResult:
    """Build a previously configured CMake consumer project."""
    build_dir = project_dir / "build"
    return _run(
        ["cmake", "--build", str(build_dir)],
        cwd=project_dir,
    )


def run_consumer_binary(
    project_dir: pathlib.Path,
    binary_name: str = "consumer",
    install_prefix: Optional[pathlib.Path] = None,
) -> BuildResult:
    """Run the consumer binary, setting LD_LIBRARY_PATH if needed."""
    build_dir = project_dir / "build"
    binary = build_dir / binary_name
    env = os.environ.copy()
    # Set tracing path to a temp location
    trace_dir = project_dir / "traces"
    trace_dir.mkdir(exist_ok=True)
    env["CLLTK_TRACING_PATH"] = str(trace_dir)
    # Add lib path so shared libs can be found
    if install_prefix:
        lib_dirs = [
            str(install_prefix / "lib64"),
            str(install_prefix / "lib"),
        ]
        existing = env.get("LD_LIBRARY_PATH", "")
        env["LD_LIBRARY_PATH"] = ":".join(lib_dirs + ([existing] if existing else []))
    return _run([str(binary)], cwd=build_dir, env=env)


def _pkg_config_args(
    lib_name: str, install_prefix: pathlib.Path
) -> Tuple[List[str], dict]:
    """Build pkg-config command args and env for the given install prefix."""
    env = os.environ.copy()
    pc_dirs = [
        str(install_prefix / "lib64" / "pkgconfig"),
        str(install_prefix / "lib" / "pkgconfig"),
    ]
    env["PKG_CONFIG_PATH"] = ":".join(pc_dirs)
    # Override prefix so .pc files resolve paths relative to the actual install
    prefix_arg = f"--define-variable=prefix={install_prefix}"
    return [prefix_arg], env


def run_pkg_config(
    lib_name: str,
    install_prefix: pathlib.Path,
) -> Tuple[bool, str, str]:
    """Run pkg-config for a library and return (success, cflags, libs).

    Sets PKG_CONFIG_PATH to find .pc files under the install prefix
    and overrides the prefix variable so paths resolve correctly.
    """
    extra_args, env = _pkg_config_args(lib_name, install_prefix)

    cflags_result = subprocess.run(
        ["pkg-config"] + extra_args + ["--cflags", lib_name],
        capture_output=True,
        text=True,
        env=env,
    )
    libs_result = subprocess.run(
        ["pkg-config"] + extra_args + ["--libs", lib_name],
        capture_output=True,
        text=True,
        env=env,
    )
    success = cflags_result.returncode == 0 and libs_result.returncode == 0
    return success, cflags_result.stdout.strip(), libs_result.stdout.strip()


def compile_with_pkg_config(
    source_code: str,
    lib_name: str,
    install_prefix: pathlib.Path,
) -> BuildResult:
    """Compile a C source file using pkg-config flags."""
    env = os.environ.copy()
    pc_dirs = [
        str(install_prefix / "lib64" / "pkgconfig"),
        str(install_prefix / "lib" / "pkgconfig"),
    ]
    env["PKG_CONFIG_PATH"] = ":".join(pc_dirs)

    tmpdir = pathlib.Path(tempfile.mkdtemp(prefix="clltk_pkgconfig_"))
    source_path = tmpdir / "test.c"
    source_path.write_text(source_code)
    output_path = tmpdir / "test"

    # Get flags from pkg-config (with prefix override)
    prefix_arg = f"--define-variable=prefix={install_prefix}"
    result = subprocess.run(
        ["pkg-config", prefix_arg, "--cflags", "--libs", lib_name],
        capture_output=True,
        text=True,
        env=env,
    )
    if result.returncode != 0:
        shutil.rmtree(tmpdir, ignore_errors=True)
        return BuildResult(
            success=False,
            returncode=result.returncode,
            stdout=result.stdout,
            stderr=f"pkg-config failed: {result.stderr}",
        )

    flags = result.stdout.strip().split()

    # Compile
    compile_result = _run(
        ["gcc", str(source_path), "-o", str(output_path)] + flags,
        cwd=tmpdir,
        env=env,
    )
    shutil.rmtree(tmpdir, ignore_errors=True)
    return compile_result
