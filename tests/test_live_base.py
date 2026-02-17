#!/usr/bin/env python3
# Copyright (c) 2024, International Business Machines
# SPDX-License-Identifier: BSD-2-Clause-Patent

"""
Base class and utilities for CLLTK live command tests.
"""

import contextlib
import os
import signal
import subprocess
import tempfile
import time
import unittest

import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.realpath(__file__))))

from helpers.base import get_build_dir, is_asan_build
from helpers.clltk_cmd import clltk


def get_clltk_path():
    """Get path to clltk executable."""
    return get_build_dir() / "command_line_tool" / "clltk"


class LiveTestCase(unittest.TestCase):
    """Base class with common setUp/tearDown for live tests."""

    def setUp(self):
        """Create temporary directory for tracebuffers."""
        self.tmp_dir = tempfile.TemporaryDirectory()
        self.trace_path = self.tmp_dir.name
        self.old_env = os.environ.get("CLLTK_TRACING_PATH")
        os.environ["CLLTK_TRACING_PATH"] = self.trace_path

    def tearDown(self):
        """Clean up."""
        if self.old_env:
            os.environ["CLLTK_TRACING_PATH"] = self.old_env
        else:
            os.environ.pop("CLLTK_TRACING_PATH", None)
        self.tmp_dir.cleanup()


@contextlib.contextmanager
def live_process(
    trace_path, order_delay=50, poll_interval=5, extra_args=None, use_stdbuf=False
):
    """
    Context manager for running live decoder subprocess.

    Args:
        trace_path: Path to trace directory
        order_delay: --order-delay value in ms
        poll_interval: --poll-interval value in ms
        extra_args: Additional arguments as list
        use_stdbuf: Whether to wrap with stdbuf for line buffering

    Yields:
        subprocess.Popen instance

    Example:
        with live_process(self.trace_path) as proc:
            # write tracepoints
            proc.send_signal(signal.SIGINT)
            stdout, stderr = proc.communicate(timeout=5)
    """
    clltk_path = get_clltk_path()

    args = []
    if use_stdbuf and not is_asan_build():
        args.extend(["stdbuf", "-oL"])

    args.extend(
        [
            str(clltk_path),
            "live",
            trace_path,
            "--order-delay",
            str(order_delay),
            "--poll-interval",
            str(poll_interval),
        ]
    )

    if extra_args:
        args.extend(extra_args)

    proc = subprocess.Popen(
        args, stdout=subprocess.PIPE, stderr=subprocess.PIPE, env=os.environ.copy()
    )

    try:
        yield proc
    finally:
        if proc.poll() is None:
            proc.send_signal(signal.SIGINT)
            try:
                proc.communicate(timeout=5)
            except subprocess.TimeoutExpired:
                proc.kill()
                proc.wait()


def run_live_with_timeout(
    trace_path,
    timeout_seconds=2,
    order_delay=50,
    poll_interval=5,
    extra_args=None,
    use_stdbuf=True,
):
    """
    Run live decoder with timeout and return result.

    Args:
        trace_path: Path to trace directory
        timeout_seconds: Timeout in seconds
        order_delay: --order-delay value in ms
        poll_interval: --poll-interval value in ms
        extra_args: Additional arguments as list
        use_stdbuf: Whether to use stdbuf for line buffering

    Returns:
        subprocess.CompletedProcess
    """
    clltk_path = get_clltk_path()

    args = ["timeout", str(timeout_seconds)]
    if use_stdbuf and not is_asan_build():
        args.extend(["stdbuf", "-oL"])

    args.extend(
        [
            str(clltk_path),
            "live",
            trace_path,
            "--order-delay",
            str(order_delay),
            "--poll-interval",
            str(poll_interval),
        ]
    )

    if extra_args:
        args.extend(extra_args)

    return subprocess.run(args, capture_output=True, env=os.environ.copy())
