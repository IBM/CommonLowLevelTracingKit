# Copyright (c) 2024, International Business Machines
# SPDX-License-Identifier: BSD-2-Clause-Patent


from collections import namedtuple
import subprocess
import pathlib

from robot.api.deco import keyword
from robot.api.exceptions import FatalError

ROBOT_AUTO_KEYWORDS = False


def cwd():
    if not hasattr(cwd, "value"):
        cwd.value = pathlib.Path(__file__).parent.resolve()
    return cwd.value


def base():
    if not hasattr(base, "value"):
        base.value = subprocess.run(['git', 'rev-parse', '--show-toplevel'],
                                    capture_output=True, text=True, check=True).stdout.strip()
        base.value = pathlib.Path(base.value)
    return base.value


def decoder_file():
    return base().joinpath("decoder_tool/python/clltk_decoder.py").resolve()


def clltk_cmd_file():
    return base().joinpath("build/command_line_tool/clltk").resolve()


cmd_return = namedtuple('cmd_output', ['rc', 'stdout', 'stderr'])


def run_cmd(*args, check_rc: bool = True):
    args = " ".join(args)
    args = args.replace('(', '\\(')
    args = args.replace(')', '\\)')
    out = subprocess.run(
        [args],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        cwd=base(),
        shell=True)
    returncode = out.returncode
    stdout = out.stdout.decode() if out.stdout else None
    stderr = out.stderr.decode() if out.stderr else None
    if check_rc and returncode != 0:
        raise FatalError(f'{args} failed with rc {returncode} and stderr {stderr}')
    return cmd_return(returncode, stdout, stderr)


@keyword("Write To ${filename}")
def wirte_to_file(filename, content):
    content = content.replace('\\n', '\n')
    with open(base().joinpath("./build/tests/robot.tests/" + filename), "w") as fh:
        fh.write(content)
    pass
