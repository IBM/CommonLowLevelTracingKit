#!/usr/bin/python3
# Copyright (c) 2024, International Business Machines
# SPDX-License-Identifier: BSD-2-Clause-Patent

"""
Backward-compatibility tests for clltk CLI commands.

Verifies that the old command invocation styles (used before the live decoder
was introduced in commit 0d4f31d) still work correctly after the rename in
commit c12ca22 ("cli: improve command naming").

Old invocation styles to preserve:
  buffer command:
    - Primary name was 'tb', alias 'tracebuffer'
    - Buffer name via positional 'name', --name, or -n
    - Size via positional 'size' or --size (was required)

  trace/tracepoint command:
    - Primary name was 'tp', alias 'tracepoint'
    - Buffer via positional 'tracebuffer', --tracebuffer, or --tb
    - Size via --tracebuffer-size
    - Message via positional 'message', --message, --msg, or -m
    - TID via --tid or -t
    - PID via --pid or -p
"""

import unittest
import tempfile
import os
import sys
import pathlib

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.realpath(__file__))))

from helpers.base import run_command
from helpers.clltk_cmd import clltk


def setUpModule():
    """Configure CMake and build clltk-cmd before running tests."""
    run_command("cmake --preset default")
    run_command("cmake --build --preset default --target clltk-cmd")


class TestBufferCommandBackwardsCompat(unittest.TestCase):
    """Backward-compatibility tests for the buffer/tracebuffer command."""

    def setUp(self):
        self.tmp_dir = tempfile.TemporaryDirectory()
        self.old_env = os.environ.get("CLLTK_TRACING_PATH")
        os.environ["CLLTK_TRACING_PATH"] = self.tmp_dir.name

    def tearDown(self):
        if self.old_env:
            os.environ["CLLTK_TRACING_PATH"] = self.old_env
        else:
            os.environ.pop("CLLTK_TRACING_PATH", None)
        self.tmp_dir.cleanup()

    def _trace_files(self):
        return list(pathlib.Path(self.tmp_dir.name).glob("*.clltk_trace"))

    def test_old_primary_name_tb(self):
        """Old primary subcommand name 'tb' still works."""
        result = clltk("tb", "--buffer", "TbOldName", "--size", "1KB")
        self.assertEqual(result.returncode, 0, msg=result.stderr)
        files = self._trace_files()
        self.assertEqual(len(files), 1)
        self.assertEqual(files[0].name, "TbOldName.clltk_trace")

    def test_old_alias_tracebuffer(self):
        """Old alias 'tracebuffer' still works."""
        result = clltk("tracebuffer", "--buffer", "TbAlias", "--size", "1KB")
        self.assertEqual(result.returncode, 0, msg=result.stderr)
        files = self._trace_files()
        self.assertEqual(len(files), 1)
        self.assertEqual(files[0].name, "TbAlias.clltk_trace")

    def test_old_name_option_long(self):
        """Old --name option for buffer name still works."""
        result = clltk("buffer", "--name", "OldNameOpt", "--size", "1KB")
        self.assertEqual(result.returncode, 0, msg=result.stderr)
        files = self._trace_files()
        self.assertEqual(len(files), 1)
        self.assertEqual(files[0].name, "OldNameOpt.clltk_trace")

    def test_old_name_option_short(self):
        """Old -n short option for buffer name still works."""
        result = clltk("buffer", "-n", "OldNShort", "--size", "1KB")
        self.assertEqual(result.returncode, 0, msg=result.stderr)
        files = self._trace_files()
        self.assertEqual(len(files), 1)
        self.assertEqual(files[0].name, "OldNShort.clltk_trace")

    def test_old_tb_with_old_name_option(self):
        """Old 'tb' subcommand with old --name option."""
        result = clltk("tb", "--name", "TbWithName", "--size", "1KB")
        self.assertEqual(result.returncode, 0, msg=result.stderr)
        files = self._trace_files()
        self.assertEqual(len(files), 1)
        self.assertEqual(files[0].name, "TbWithName.clltk_trace")

    def test_old_tb_with_short_n(self):
        """Old 'tb' subcommand with -n short option."""
        result = clltk("tb", "-n", "TbShortN", "-s", "1KB")
        self.assertEqual(result.returncode, 0, msg=result.stderr)
        files = self._trace_files()
        self.assertEqual(len(files), 1)
        self.assertEqual(files[0].name, "TbShortN.clltk_trace")

    def test_old_tracebuffer_with_old_name(self):
        """Old 'tracebuffer' alias with old --name option."""
        result = clltk("tracebuffer", "--name", "TbFullAlias", "--size", "1KB")
        self.assertEqual(result.returncode, 0, msg=result.stderr)
        files = self._trace_files()
        self.assertEqual(len(files), 1)
        self.assertEqual(files[0].name, "TbFullAlias.clltk_trace")


class TestTraceCommandBackwardsCompat(unittest.TestCase):
    """Backward-compatibility tests for the trace/tracepoint command."""

    def setUp(self):
        self.tmp_dir = tempfile.TemporaryDirectory()
        self.old_env = os.environ.get("CLLTK_TRACING_PATH")
        os.environ["CLLTK_TRACING_PATH"] = self.tmp_dir.name

    def tearDown(self):
        if self.old_env:
            os.environ["CLLTK_TRACING_PATH"] = self.old_env
        else:
            os.environ.pop("CLLTK_TRACING_PATH", None)
        self.tmp_dir.cleanup()

    def _trace_files(self):
        return list(pathlib.Path(self.tmp_dir.name).glob("*.clltk_trace"))

    def test_old_primary_name_tp(self):
        """Old primary subcommand name 'tp' still works."""
        result = clltk("tp", "TpBuffer", "hello from tp")
        self.assertEqual(result.returncode, 0, msg=result.stderr)
        files = self._trace_files()
        self.assertEqual(len(files), 1)
        self.assertEqual(files[0].name, "TpBuffer.clltk_trace")

    def test_old_alias_tracepoint(self):
        """Old alias 'tracepoint' still works."""
        result = clltk("tracepoint", "TpAliasBuffer", "hello from tracepoint")
        self.assertEqual(result.returncode, 0, msg=result.stderr)
        files = self._trace_files()
        self.assertEqual(len(files), 1)
        self.assertEqual(files[0].name, "TpAliasBuffer.clltk_trace")

    def test_old_tracebuffer_option_long(self):
        """Old --tracebuffer option for buffer name still works."""
        result = clltk("trace", "--tracebuffer", "OldTbOpt", "--message", "msg")
        self.assertEqual(result.returncode, 0, msg=result.stderr)

    def test_old_tracebuffer_option_tb(self):
        """Old --tb short long-option for buffer name still works."""
        result = clltk("trace", "--tb", "OldTbShort", "--message", "msg")
        self.assertEqual(result.returncode, 0, msg=result.stderr)

    def test_old_tracebuffer_size_option(self):
        """Old --tracebuffer-size option still works."""
        result = clltk("trace", "TsizeBuffer", "msg", "--tracebuffer-size", "1KB")
        self.assertEqual(result.returncode, 0, msg=result.stderr)

    def test_old_message_option_msg(self):
        """Old --msg option for message still works."""
        result = clltk("trace", "MsgOptBuffer", "--msg", "message via --msg")
        self.assertEqual(result.returncode, 0, msg=result.stderr)

    def test_old_message_option_long(self):
        """Old --message long option still works."""
        result = clltk(
            "trace", "MessageOptBuffer", "--message", "message via --message"
        )
        self.assertEqual(result.returncode, 0, msg=result.stderr)

    def test_old_tid_short_option(self):
        """Old -t short option for TID still works."""
        result = clltk("trace", "TidBuffer", "tid test", "-t", "42")
        self.assertEqual(result.returncode, 0, msg=result.stderr)

    def test_old_pid_short_option(self):
        """Old -p short option for PID still works."""
        result = clltk("trace", "PidBuffer", "pid test", "-p", "99")
        self.assertEqual(result.returncode, 0, msg=result.stderr)

    def test_old_tp_with_old_tracebuffer_option(self):
        """Old 'tp' subcommand with old --tracebuffer option."""
        result = clltk("tp", "--tracebuffer", "TpOldOpts", "--message", "old style")
        self.assertEqual(result.returncode, 0, msg=result.stderr)

    def test_old_tp_with_all_old_options(self):
        """Old 'tp' with all old-style options: --tracebuffer, --msg, -t, -p."""
        result = clltk(
            "tp",
            "--tracebuffer",
            "TpAllOld",
            "--msg",
            "full old-style message",
            "--tracebuffer-size",
            "2KB",
            "-t",
            "123",
            "-p",
            "456",
            "--file",
            "test.c",
            "--line",
            "77",
        )
        self.assertEqual(result.returncode, 0, msg=result.stderr)
        files = self._trace_files()
        self.assertEqual(len(files), 1)
        self.assertEqual(files[0].name, "TpAllOld.clltk_trace")

    def test_old_tracepoint_alias_with_old_options(self):
        """Old 'tracepoint' alias with all old-style options."""
        result = clltk(
            "tracepoint",
            "--tracebuffer",
            "TpointAllOld",
            "--msg",
            "tracepoint alias old-style",
            "-t",
            "10",
            "-p",
            "20",
        )
        self.assertEqual(result.returncode, 0, msg=result.stderr)
        files = self._trace_files()
        self.assertEqual(len(files), 1)
        self.assertEqual(files[0].name, "TpointAllOld.clltk_trace")


if __name__ == "__main__":
    unittest.main()
