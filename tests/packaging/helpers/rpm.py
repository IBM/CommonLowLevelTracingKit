# Copyright (c) 2024, International Business Machines
# SPDX-License-Identifier: BSD-2-Clause-Patent

"""Helper utilities for RPM package inspection and validation."""

import pathlib
import re
import subprocess
from dataclasses import dataclass, field
from typing import Dict, List, Optional

from tests.helpers.base import get_repo_root, get_build_dir


def get_packages_dir() -> pathlib.Path:
    """Get the directory where CPack writes RPM packages."""
    return get_build_dir() / "packages"


EXPECTED_RPM_PATTERNS = [
    "clltk-tracing-",
    "clltk-decoder-",
    "clltk-snapshot-",
    "clltk-devel-",
    "clltk-cmd-",
    "clltk-python-decoder-",
]


def packages_exist() -> bool:
    """Check if all expected RPM packages have been built."""
    pkg_dir = get_packages_dir()
    if not pkg_dir.is_dir():
        return False
    rpm_names = [p.name for p in pkg_dir.glob("*.rpm")]
    if not rpm_names:
        return False
    # Check that every expected subpackage has at least one matching RPM
    for pattern in EXPECTED_RPM_PATTERNS:
        if not any(pattern in name for name in rpm_names):
            return False
    return True


def ensure_rpms_built() -> None:
    """Build RPMs if they don't exist yet."""
    if packages_exist():
        return
    root = get_repo_root()
    # Always (re-)configure with the rpm preset using --fresh so that any
    # leftover CMakeCache.txt from a different preset (e.g. "unittests") is
    # discarded.  Without --fresh the old cache variables (missing
    # CPACK_GENERATOR=RPM) survive and CPack produces TGZ/Shell archives
    # instead of RPMs.
    subprocess.run(
        ["cmake", "--preset", "rpm", "--fresh"],
        cwd=root,
        check=True,
        capture_output=True,
    )
    # Build
    subprocess.run(
        ["cmake", "--build", "--preset", "rpm"],
        cwd=root,
        check=True,
        capture_output=True,
    )
    # Package binary RPMs via CPack presets
    for preset in [
        "rpm-libs",
        "rpm-devel",
        "rpm-cmd",
        "rpm-python",
        "rpm-full",
    ]:
        subprocess.run(
            ["cpack", "--preset", preset],
            cwd=root,
            check=True,
            capture_output=True,
        )
    # Build SRPM via custom target (uses git archive + rpmbuild -bs)
    subprocess.run(
        ["cmake", "--build", "--preset", "rpm", "--target", "srpm"],
        cwd=root,
        check=True,
        capture_output=True,
    )


def find_rpms(pattern: str = "*.rpm") -> List[pathlib.Path]:
    """Find RPM files matching a glob pattern in the packages directory."""
    pkg_dir = get_packages_dir()
    return sorted(pkg_dir.glob(pattern))


def find_binary_rpms() -> List[pathlib.Path]:
    """Find all binary (non-source) RPMs."""
    return [p for p in find_rpms("*.rpm") if not p.name.endswith(".src.rpm")]


def find_srpms() -> List[pathlib.Path]:
    """Find all source RPMs."""
    return find_rpms("*.src.rpm")


@dataclass
class RpmInfo:
    """Parsed RPM package metadata."""

    name: str = ""
    version: str = ""
    release: str = ""
    architecture: str = ""
    license: str = ""
    summary: str = ""
    description: str = ""
    vendor: str = ""
    url: str = ""
    raw: str = ""


@dataclass
class RpmContents:
    """RPM package file listing and metadata."""

    info: RpmInfo = field(default_factory=RpmInfo)
    files: List[str] = field(default_factory=list)
    requires: List[str] = field(default_factory=list)
    provides: List[str] = field(default_factory=list)
    scripts: Dict[str, str] = field(default_factory=dict)


def _run_rpm(args: List[str]) -> str:
    """Run an rpm command and return stdout."""
    result = subprocess.run(
        ["rpm"] + args,
        capture_output=True,
        text=True,
        check=True,
    )
    return result.stdout.strip()


def rpm_query_info(rpm_path: pathlib.Path) -> RpmInfo:
    """Query RPM metadata (name, version, license, etc.)."""
    raw = _run_rpm(["-qpi", str(rpm_path)])
    info = RpmInfo(raw=raw)
    for line in raw.splitlines():
        if ":" not in line:
            continue
        key, _, value = line.partition(":")
        key = key.strip().lower()
        value = value.strip()
        if key == "name":
            info.name = value
        elif key == "version":
            info.version = value
        elif key == "release":
            info.release = value
        elif key == "architecture":
            info.architecture = value
        elif key == "license":
            info.license = value
        elif key == "summary":
            info.summary = value
        elif key == "vendor":
            info.vendor = value
        elif key == "url":
            info.url = value
        elif key == "description":
            info.description = value
    return info


def rpm_query_filelist(rpm_path: pathlib.Path) -> List[str]:
    """List all files in an RPM."""
    output = _run_rpm(["-qpl", str(rpm_path)])
    if not output:
        return []
    return output.splitlines()


def rpm_query_requires(rpm_path: pathlib.Path) -> List[str]:
    """List all dependencies of an RPM."""
    output = _run_rpm(["-qpR", str(rpm_path)])
    if not output:
        return []
    return [r.strip() for r in output.splitlines() if r.strip()]


def rpm_query_provides(rpm_path: pathlib.Path) -> List[str]:
    """List all provides of an RPM."""
    output = _run_rpm(["-qp", "--provides", str(rpm_path)])
    if not output:
        return []
    return [p.strip() for p in output.splitlines() if p.strip()]


def rpm_query_scripts(rpm_path: pathlib.Path) -> Dict[str, str]:
    """Query pre/post install/uninstall scriptlets."""
    output = _run_rpm(["-qp", "--scripts", str(rpm_path)])
    scripts = {}
    current_section = None
    current_lines = []
    for line in output.splitlines():
        # Section headers look like "postinstall scriptlet (using /bin/sh):"
        match = re.match(
            r"^(preinstall|postinstall|preuninstall|postuninstall)\s+scriptlet", line
        )
        if match:
            if current_section and current_lines:
                scripts[current_section] = "\n".join(current_lines)
            current_section = match.group(1)
            current_lines = []
        elif current_section is not None:
            current_lines.append(line)
    if current_section and current_lines:
        scripts[current_section] = "\n".join(current_lines)
    return scripts


def rpm_inspect(rpm_path: pathlib.Path) -> RpmContents:
    """Full inspection of an RPM: metadata, files, deps, scripts."""
    return RpmContents(
        info=rpm_query_info(rpm_path),
        files=rpm_query_filelist(rpm_path),
        requires=rpm_query_requires(rpm_path),
        provides=rpm_query_provides(rpm_path),
        scripts=rpm_query_scripts(rpm_path),
    )


def get_version_string() -> str:
    """Read the CLLTK version from VERSION.md."""
    version_file = get_repo_root() / "VERSION.md"
    first_line = version_file.read_text().splitlines()[0].strip()
    return first_line


def find_rpm_by_name(name_pattern: str) -> Optional[pathlib.Path]:
    """Find a single RPM whose filename matches a pattern.

    Excludes source RPMs. Returns None if not found.
    """
    for rpm_path in find_binary_rpms():
        if re.search(name_pattern, rpm_path.name):
            return rpm_path
    return None


def install_rpms_to_prefix(rpm_paths: List[pathlib.Path], prefix: pathlib.Path) -> None:
    """Install RPMs to a temporary prefix using rpm --relocate.

    This uses rpm2cpio + cpio to extract files without needing root.
    """
    prefix.mkdir(parents=True, exist_ok=True)
    for rpm_path in rpm_paths:
        result = subprocess.run(
            f"rpm2cpio {rpm_path} | cpio -idmv",
            shell=True,
            cwd=prefix,
            capture_output=True,
        )
        if result.returncode != 0:
            raise RuntimeError(
                f"Failed to extract {rpm_path.name}: {result.stderr.decode()}"
            )


def cmake_install_to_prefix(prefix: pathlib.Path) -> None:
    """Run cmake --install to install all components to a prefix."""
    build_dir = get_build_dir()
    subprocess.run(
        ["cmake", "--install", str(build_dir), "--prefix", str(prefix)],
        check=True,
        capture_output=True,
    )
