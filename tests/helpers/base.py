# Copyright (c) 2024, International Business Machines
# SPDX-License-Identifier: BSD-2-Clause-Patent

"""Core utilities for test infrastructure."""

import subprocess
import pathlib
import os
from dataclasses import dataclass
from typing import Optional, Union, List


@dataclass
class CommandResult:
    """Result of a command execution."""
    returncode: int
    stdout: str
    stderr: str


def get_repo_root() -> pathlib.Path:
    """Get the repository root path."""
    result = subprocess.run(
        ['git', 'rev-parse', '--show-toplevel'],
        capture_output=True, text=True, check=True
    )
    return pathlib.Path(result.stdout.strip())


def get_build_dir() -> pathlib.Path:
    """Get the build directory path."""
    return pathlib.Path(os.environ.get('BUILD_DIR', './build/')).resolve()


def run_command(
    command: Union[str, List[str]],
    cwd: Optional[pathlib.Path] = None,
    env: Optional[dict] = None,
    check: bool = True
) -> CommandResult:
    """
    Execute a shell command and return results.

    Args:
        command: Command string or list of arguments
        cwd: Working directory for the command
        env: Environment variables
        check: If True, raise exception on non-zero return code

    Returns:
        CommandResult with returncode, stdout, and stderr
    """
    if cwd is None:
        cwd = get_repo_root()

    use_shell = isinstance(command, str)
    
    if use_shell:
        # Escape parentheses for shell
        command = command.replace('(', '\\(')
        command = command.replace(')', '\\)')

    out = subprocess.run(
        command,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        cwd=cwd,
        env=env,
        shell=use_shell
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


def decoder_file() -> pathlib.Path:
    """Get path to the decoder Python script."""
    return get_repo_root() / "decoder_tool" / "python" / "clltk_decoder.py"


def clltk_cmd_file() -> pathlib.Path:
    """Get path to the clltk command-line tool."""
    return get_build_dir() / "command_line_tool" / "clltk"
