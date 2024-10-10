# Copyright (c) 2024, International Business Machines
# SPDX-License-Identifier: BSD-2-Clause-Patent


import base


from robot.api.deco import keyword

ROBOT_AUTO_KEYWORDS = False


@keyword("get root folder")
def get_root_folder():
    return base.base()

@keyword("get build folder")
def get_build_folder():
    return base.base() / 'build'

@keyword("configure cmake and build default")
def config_cmake():
    base.run_cmd("cmake --preset default")
    base.run_cmd(f'cmake --build --preset default --target clltk-cmd')
    pass


@keyword('Build Cmake Target ${target_name}')
def build_target(target_name: str, check_rc: bool = True):
    return base.run_cmd(f'cmake --build --preset default --target {target_name}', check_rc=check_rc)
