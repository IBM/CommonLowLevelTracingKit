# Copyright (c) 2024, International Business Machines
# SPDX-License-Identifier: BSD-2-Clause-Patent

#%%

import pathlib
import subprocess
from collections import namedtuple
import json
import os
import tempfile
import unittest
import pandas as pd
from enum import Enum

class Langauge(Enum):
    C = 0
    CPP = 1

returnType = namedtuple("returnValue", ["returncode", "stdout", "stderr"])
helper_dir_path = pathlib.Path(__file__).parent.resolve()
temp_target_dir_path = helper_dir_path.joinpath("./temp_target/").resolve()
assert temp_target_dir_path.exists() and temp_target_dir_path.is_dir(), "failed to get temp_target folder"
decoder_file = helper_dir_path.joinpath("./../../../decoder_tool/python/clltk_decoder.py").resolve()
assert decoder_file.is_file(), "decoder not found"
BUILD_DIR=os.path.realpath(os.environ.get('BUILD_DIR', './build/') + '/temp_target/')

        
def _run(*args, env=os.environ):
    args = " ".join(args)
    args = args.replace('(','\\(')
    args = args.replace(')','\\)')
    
    out = subprocess.run(
        [args],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        cwd=temp_target_dir_path,
        env = env,
        shell=True)
    if 0 != out.returncode:
        raise RuntimeError(f"rc = {out.returncode:d} stderr = {out.stderr.decode():s}")
    if "" != out.stderr.decode():
        raise RuntimeError(out.stderr.decode())
    stdout = out.stdout.decode()
    return stdout

def process(file_content, build_musst_fail=False, run_musst_fail=False, runs=1, language=Langauge.C, check_callback = None):
    target = {Langauge.C:"main_c", Langauge.CPP :"main_cpp"}[language]
    file = {Langauge.C:"main.gen.c", Langauge.CPP :"main.gen.cpp"}[language]
    
    tmp = tempfile.TemporaryDirectory()
    env ={str(key):value for key,value in  os.environ.items() if "CLLTK" not in str(key)}
    env['CLLTK_TRACING_PATH'] = tmp.name

    _run(f"cmake -S . -B {BUILD_DIR} -G \"Unix Makefiles\"")
    with open(temp_target_dir_path.joinpath(f"{BUILD_DIR}/{file}"), "w") as fh:
        fh.write(file_content)

    try:
        _run(f"cmake --build {BUILD_DIR} --target {target}")
    except RuntimeError as e:
        if build_musst_fail:
            tmp.cleanup()
            return None
        else:
            raise e
    if build_musst_fail:
        raise RuntimeError("build must fail but did not")

    try:
        for _ in range(runs):
            _run(f"{BUILD_DIR}/{target} {tmp.name}", env=env)
    except RuntimeError as e:
        if run_musst_fail:
            tmp.cleanup()
            return e.args[0]
        else:
            raise e
    if run_musst_fail:
        raise RuntimeError("run must fail but did not")

    if(check_callback is not None):
        check_return = check_callback(tmp)
        assert check_return is None or check_return
    
    trace_files = " ".join(tmp.name + "/" + file for file in os.listdir(tmp.name))


    _run(f"{str(decoder_file)} -o {BUILD_DIR}/output.csv {trace_files}")
    if os.path.getsize(temp_target_dir_path.joinpath(f"{BUILD_DIR}/output.csv")):
        tracepoints = pd.read_csv(temp_target_dir_path.joinpath(f"{BUILD_DIR}/output.csv"))
    else:
        tracepoints = None
    tmp.cleanup()

    return tracepoints
    
# %%
