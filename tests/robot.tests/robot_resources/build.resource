*** Settings ***
Library     base.py
Library     build.py


*** Keywords ***
source ${content} for ${language} fails with
    [Arguments]    ${expected_error}

    Write To main.gen.c    int main(){ return 0; }
    Write To main.gen.cpp    int main(){ return 0; }

    IF    '${language}' == 'C'
        Write To main.gen.c    ${content}
        ${output}=    Build Cmake Target robot_tests_exe_c    check_rc=${False}
        Should Not Be Equal As Integers    ${output.rc}    0
        Should Contain    ${output.stderr}    ${expected_error}
    ELSE IF    '${language}' == 'CPP'
        Write To main.gen.cpp    ${content}
        ${output}=    Build Cmake Target robot_tests_exe_cpp    check_rc=${False}
        Should Not Be Equal As Integers    ${output.rc}    0
        Should Contain    ${output.stderr}    ${expected_error}
    ELSE
        Fatal Error
    END
