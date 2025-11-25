*** Settings ***
Resource            ./../robot_resources/build.resource
Resource            ./../robot_resources/runtime.resource

Suite Setup         configure cmake and build default
Test Setup          set up env
Test Teardown       clean up env


*** Test Cases ***
clltk-cmd without args
    ${output}=    CLLTK
    Should Be Equal As Integers    ${output.rc}    0
    Should Contain    ${output.stdout}    clltk [OPTIONS] [SUBCOMMAND]

clltk-cmd with help
    ${output}=    CLLTK    --help
    Should Be Equal As Integers    ${output.rc}    0
    Should Contain    ${output.stdout}    clltk [OPTIONS] [SUBCOMMAND]
    ${output}=    CLLTK    -h
    Should Be Equal As Integers    ${output.rc}    0
    Should Contain    ${output.stdout}    clltk [OPTIONS] [SUBCOMMAND]

clltk-cmd version
    ${output}=    CLLTK    --version
    Should Be Equal As Integers    ${output.rc}    0
    Should Match Regexp    ${output.stdout}    Common Low Level Tracing Kit \\d+.\\d+.\\d+
