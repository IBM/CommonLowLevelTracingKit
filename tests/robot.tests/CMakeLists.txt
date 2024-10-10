# Copyright (c) 2024, International Business Machines
# SPDX-License-Identifier: BSD-2-Clause-Patent


# robot_tests_exe_c and robot_tests_exe_cpp are executables used for robot tests

set(main_c_file ${CMAKE_CURRENT_BINARY_DIR}/main.gen.c)
file(WRITE ${main_c_file} "int main(void){return 0;} // default")

add_executable(robot_tests_exe_c
    ${main_c_file}
)

target_link_libraries(robot_tests_exe_c
    clltk_tracing_shared
)

set_property(TARGET robot_tests_exe_c PROPERTY COMPILE_WARNING_AS_ERROR ON)


set(main_cpp_file ${CMAKE_CURRENT_BINARY_DIR}/main.gen.cpp)
file(WRITE ${main_cpp_file} "int main(void){return 0;} // default")

add_executable(robot_tests_exe_cpp
    ${main_cpp_file}
)

target_link_libraries(robot_tests_exe_cpp
    clltk_tracing_shared
)

set_property(TARGET robot_tests_exe_cpp PROPERTY COMPILE_WARNING_AS_ERROR ON)
