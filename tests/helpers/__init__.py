# Copyright (c) 2024, International Business Machines
# SPDX-License-Identifier: BSD-2-Clause-Patent

"""
Test helpers module for CommonLowLevelTracingKit.

Provides utilities for:
- Base operations (paths, subprocess execution)
- CMake build operations
- Runtime execution and environment management
- CLLTK CLI command execution
- Library validation (PIC, relocatable checks)
"""

from .base import (
    get_repo_root,
    get_build_dir,
    run_command,
    CommandResult,
)
from .clltk_cmd import clltk
from .library_validation import (
    is_static_lib_relocatable,
    is_shared_lib_pic,
)
