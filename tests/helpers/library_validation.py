# Copyright (c) 2024, International Business Machines
# SPDX-License-Identifier: BSD-2-Clause-Patent

"""Library inspection utilities for build validation."""

import pathlib
import tempfile
from .base import run_command


def is_static_lib_relocatable(lib_path: pathlib.Path) -> bool:
    """
    Check if all objects in a static library are relocatable.

    Args:
        lib_path: Path to the static library (.a file)

    Returns:
        True if all objects are relocatable, False otherwise
    """
    with tempfile.TemporaryDirectory() as tmp_dir:
        result = run_command(
            f"ar -xv --output={tmp_dir} {lib_path}",
            check=False
        )
        if result.returncode != 0:
            return False

        for line in result.stdout.strip().split('\n'):
            if not line:
                continue
            parts = line.split()
            if len(parts) >= 3:
                obj_file = pathlib.Path(tmp_dir) / parts[2]
                file_result = run_command(f"file {obj_file}", check=False)
                if file_result.returncode != 0:
                    return False
                if "relocatable" not in file_result.stdout:
                    return False

    return True


def is_shared_lib_pic(lib_path: pathlib.Path) -> bool:
    """
    Check if a shared library is Position Independent Code (PIC).

    A shared library is considered PIC if it does not contain TEXTREL
    (text relocations), which would indicate non-PIC code.

    Args:
        lib_path: Path to the shared library (.so file)

    Returns:
        True if the library is PIC, False otherwise
    """
    result = run_command(f"readelf -d {lib_path}", check=False)
    if result.returncode != 0:
        return False
    return "TEXTREL" not in result.stdout
