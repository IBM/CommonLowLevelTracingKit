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


class unittest(unittest.TestCase):
    root: pathlib.Path = None
    target: str = None
    decoded: pd.DataFrame = None
    tmp_folder : tempfile.TemporaryDirectory = None
    env : dict = {}

    def setUp(self) -> None:
        assert isinstance(self.target, str), f"target not set ({self.target})"
        assert pathlib.Path(self.root).resolve().is_dir(), f"root not a folder ({self.root})"
        self.trace_folder = self.root.joinpath("build", "traces", self.target).absolute()
        os.makedirs(self.trace_folder, exist_ok=True)
        assert self.trace_folder.is_dir() and self.trace_folder.exists()
        self.build_target()
        pass

    def setUp(self):
        self.tmp_folder = tempfile.TemporaryDirectory()
        self.env = {**os.environ , "CLLTK_TRACING_PATH": self.tmp_folder.name}
        self.decoded = self.decode_traces()
        self.assertTrue(self.decoded.empty), "could not removed old traces"
        self.run_target()
        self.decoded = self.decode_traces()
        self.assertTrue(not self.decoded.empty), "missing new traces"
        return

    def build_target(self):
        command = [f"{self.root}/scripts/userspace/build.sh", "--target", f"{self.target}"]
        r = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        assert r.returncode == 0, r.stderr
        pass

    def run_target(self):
        command = [f"{self.root}/scripts/userspace/run.sh", f"{self.target}"]
        r = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE, env=self.env)
        assert r.returncode == 0 and not r.stderr, r.stderr
        return r.stdout.decode("utf-8").strip()


    def decode_traces(self):
        path_to_decoder = self.root.joinpath("decoder_tool/python/clltk_decoder.py").resolve()
        assert path_to_decoder.is_file()
        command = [
            str(path_to_decoder),
            "-o",
            str(self.tmp_folder.name) + f"/{self.target}.csv",
            str(self.tmp_folder.name)]
        r = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE, env=self.env)
        assert r.returncode == 0 and not r.stderr, f"decoding failed with sterr: \"{r.stderr}\" and stdout: \"{r.stdout}\""
        if os.stat(str(self.tmp_folder.name) + f"/{self.target}.csv").st_size:
            data = pd.read_csv(str(self.tmp_folder.name) + f"/{self.target}.csv")
            return data
        else:
            return None

    def tearDown(self):
        self.tmp_folder.cleanup()

# %%
