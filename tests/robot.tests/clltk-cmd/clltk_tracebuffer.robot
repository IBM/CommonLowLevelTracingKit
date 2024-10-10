*** Settings ***
Resource            ./../robot_resources/build.resource
Resource            ./../robot_resources/runtime.resource
Library             Collections
Library             String

Suite Setup         configure cmake and build default
Test Setup          set up env
Test Teardown       clean up env

*** Keywords ***
Invalid tracebuffer name ${name}
    ${rc}=    CLLTK    tracebuffer --name ${name} --size 256    check_rc=${False}
    Should Not Be Equal As Integers    ${rc.rc}    0    msg=naming tracebuffer ${name} should fail

Valid tracebuffer name ${name}
    ${name}=    Replace String    ${name}    \"    replace_with=   
    ${rc}=    CLLTK    tracebuffer --name ${name} --size 256

    ${files}=     List Files In Directory    ${CLLTK_TRACING_PATH}    pattern=*.clltk_trace
    ${files_length}=    Get Length  ${files}
    Should Be Equal As Integers    ${files_length}    1    msg=${files}
    Should Be Equal As Strings    ${files}[0]    ${name}.clltk_trace

    Clean Up Directory ${CLLTK_TRACING_PATH}


Valid tracebuffer size ${size}
    ${rc}=    CLLTK    tracebuffer --name Buffer --size ${size}
    Should Be Equal As Integers    ${rc.rc}    0    msg=creating tracebuffer with size ${size} should not fail

    ${files}=     List Files In Directory    ${CLLTK_TRACING_PATH}    pattern=*.clltk_trace
    ${files_length}=    Get Length  ${files}
    Should Be Equal As Integers    ${files_length}    1    msg=${files}
    Should Be Equal As Strings    ${files}[0]    Buffer.clltk_trace

    Clean Up Directory ${CLLTK_TRACING_PATH}

*** Test Cases ***

Check that CLLTK_TRACING_PATH is empty by default
    ${files}=     List Files In Directory    ${CLLTK_TRACING_PATH}    pattern=*.clltk_trace
    ${files_length}=    Get Length  ${files}
    Should Be Equal As Integers    ${files_length}    0


Create tracebuffer
    ${rc}=    CLLTK    tracebuffer --name MyFirstTracebuffer --size 1KB
    Should Be Equal As Integers    ${rc.rc}    0    msg=${rc.stderr}

    ${files}=     List Files In Directory    ${CLLTK_TRACING_PATH}    pattern=*.clltk_trace
    ${files_length}=    Get Length  ${files}
    Should Be Equal As Integers    ${files_length}    1    msg=${files}
    Should Be Equal As Strings    ${files}[0]    MyFirstTracebuffer.clltk_trace


Invalid tracebuffer name
    [Template]    Invalid tracebuffer name ${name}
    " Buffer"
    "Buffer "
    "_Buffer"
    "8uffer"
    "Buffe~r"
    "Buffe#r"
    "Buffe'r"

Valid tracebuffer name
    [Template]    Valid tracebuffer name ${name}
    "MyFirstTracebuffer"
    "B_ffer"
    "BuffEr"

Valid tracebuffer size
    [Template]    Valid tracebuffer size ${size}
    "1MB"
    "10KB"
    "1024"
    "10"

Check tracing path

    Create Directory    ${CLLTK_TRACING_PATH}/NewDirA
    Set Environment Variable    CLLTK_TRACING_PATH    ${CLLTK_TRACING_PATH}/NewDirA
    ${rc}=    CLLTK  -C ${CLLTK_TRACING_PATH}/NewDirA tracebuffer --name BufferInA --size 1KB
    Should Be Equal As Integers    ${rc.rc}    0    msg=${rc.stderr}
    
    Create Directory    ${CLLTK_TRACING_PATH}/NewDirB
    Set Environment Variable    CLLTK_TRACING_PATH    ${CLLTK_TRACING_PATH}/NewDirB
    ${rc}=    CLLTK  -C ${CLLTK_TRACING_PATH}/NewDirB  tracebuffer --name BufferInB --size 1KB
    Should Be Equal As Integers    ${rc.rc}    0    msg=${rc.stderr}

    ${files}=     List Files In Directory    ${CLLTK_TRACING_PATH}/NewDirA    pattern=*.clltk_trace
    ${files_length}=    Get Length  ${files}
    Should Be Equal As Integers    ${files_length}    1    msg=${files}
    Should Be Equal As Strings    ${files}[0]    BufferInA.clltk_trace

    ${files}=     List Files In Directory    ${CLLTK_TRACING_PATH}/NewDirB    pattern=*.clltk_trace
    ${files_length}=    Get Length  ${files}
    Should Be Equal As Integers    ${files_length}    1    msg=${files}
    Should Be Equal As Strings    ${files}[0]    BufferInB.clltk_trace
