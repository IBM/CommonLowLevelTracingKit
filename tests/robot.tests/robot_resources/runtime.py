# Copyright (c) 2024, International Business Machines
# SPDX-License-Identifier: BSD-2-Clause-Patent


import os
import tempfile
import re
import base

from robot.api.deco import keyword

ROBOT_AUTO_KEYWORDS = False


@keyword("Create Temporary Directory")
def create_tmp():
    return tempfile.TemporaryDirectory()

@keyword("Remove Temporary Directory")
def remove_tmp(tmp:tempfile.TemporaryDirectory):
    tmp.cleanup()
    pass


@keyword("Run Cmake Target ${target_name}")
def run_target(target_name, *args, check_rc: bool = True):
    location = base.run_cmd(f'find ./build/ -type f -name {target_name} -not -path "./build/in_container/*"')
    assert location.rc == 0
    location = location.stdout.split('\n')
    assert len(location) == 2 and location[1] == '', f"exe not found {location}" 
    return base.run_cmd(" ".join([location[0],*args]), check_rc=check_rc)

@keyword("Decode Tracebuffer")
def decode_tracepoints():
    output = base.run_cmd(f"{base.decoder_file()} -o $CLLTK_TRACING_PATH/output.txt $CLLTK_TRACING_PATH")
    assert output.rc == 0, "Decode Tracebuffer failed with: " + output.stderr
    with open(os.getenv('CLLTK_TRACING_PATH') + "/output.txt", 'r') as fh:
        return fh.read()

@keyword("exist tracepoints")
def exist_tracpoints(tracepoints, count: int, search : str):
    matches = re.findall(search, tracepoints)
    if count > 0 :
        assert len(matches) == count, f"could not found the right number of tracepoints for {search} {len(matches)} != {count}"
    else:
        assert len(matches), f"could not found any tracepoints for {search}"


@keyword("CLLTK")
def clltk_cmd(*args, check_rc: bool = True):
    return base.run_cmd(" ".join([str(base.clltk_cmd_file()), *args]), check_rc=check_rc)

