# Copyright (c) 2024, International Business Machines
# SPDX-License-Identifier: BSD-2-Clause-Patent

"""CLLTK CLI command wrapper."""

import subprocess
import pathlib
from dataclasses import dataclass
from typing import Optional
from .base import clltk_cmd_file, get_repo_root, CommandResult


def clltk(
    *args: str,
    check: bool = True,
    env: Optional[dict] = None,
    cwd: Optional[pathlib.Path] = None,
) -> CommandResult:
    """
    Execute the clltk command-line tool.

    Args:
        *args: Command-line arguments to pass to clltk
        check: If True, raise exception on non-zero return code
        env: Optional environment variables
        cwd: Working directory (default: current directory)

    Returns:
        CommandResult with returncode, stdout, and stderr
    """
    cmd_path = clltk_cmd_file()
    command = [str(cmd_path)] + list(args)

    import os

    if env is None:
        env = os.environ.copy()

    out = subprocess.run(
        command, stdout=subprocess.PIPE, stderr=subprocess.PIPE, cwd=cwd, env=env
    )

    stdout = out.stdout.decode() if out.stdout else ""
    stderr = out.stderr.decode() if out.stderr else ""

    if check and out.returncode != 0:
        raise RuntimeError(
            f"Command failed with rc {out.returncode}\n"
            f"Command: {command}\n"
            f"stderr: {stderr}"
        )

    return CommandResult(out.returncode, stdout, stderr)
