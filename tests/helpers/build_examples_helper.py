#!/usr/bin/python3
# Copyright (c) 2024, International Business Machines
# SPDX-License-Identifier: BSD-2-Clause-Patent

# %%
import unittest
import subprocess
import pathlib
import os
import pandas as pd
import tempfile

from .base import get_repo_root


class ExamplesTestCase(unittest.TestCase):
    root: pathlib.Path = None
    target: str = None
    decoded: pd.DataFrame = None
    tmp_folder: tempfile.TemporaryDirectory = None
    env: dict = {}

    def setUp(self):
        if self.root is None:
            self.root = get_repo_root()
        self.tmp_folder = tempfile.TemporaryDirectory()
        self.env = {**os.environ, "CLLTK_TRACING_PATH": self.tmp_folder.name}
        self.decoded = self.decode_traces()
        self.assertTrue(self.decoded.empty), "could not removed old traces"
        self.run_target()
        self.decoded = self.decode_traces()
        self.assertTrue(not self.decoded.empty), "missing new traces"
        return

    def build_target(self):
        command = [
            f"{self.root}/scripts/userspace/build.sh",
            "--target",
            f"{self.target}",
        ]
        r = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        assert r.returncode == 0, r.stderr
        pass

    def run_target(self):
        command = [f"{self.root}/scripts/userspace/run.sh", f"{self.target}"]
        r = subprocess.run(
            command, stdout=subprocess.PIPE, stderr=subprocess.PIPE, env=self.env
        )
        assert r.returncode == 0 and not r.stderr, r.stderr
        return r.stdout.decode("utf-8").strip()

    def decode_traces(self):
        path_to_decoder = self.root.joinpath(
            "decoder_tool/python/clltk_decoder.py"
        ).resolve()
        assert path_to_decoder.is_file()
        command = [
            str(path_to_decoder),
            "-o",
            str(self.tmp_folder.name) + f"/{self.target}.csv",
            str(self.tmp_folder.name),
        ]
        r = subprocess.run(
            command, stdout=subprocess.PIPE, stderr=subprocess.PIPE, env=self.env
        )
        assert r.returncode == 0 and not r.stderr, (
            f'decoding failed with sterr: "{r.stderr}" and stdout: "{r.stdout}"'
        )
        if os.stat(str(self.tmp_folder.name) + f"/{self.target}.csv").st_size:
            data = pd.read_csv(str(self.tmp_folder.name) + f"/{self.target}.csv")
            return data
        else:
            return None

    def tearDown(self):
        self.tmp_folder.cleanup()


# %%
