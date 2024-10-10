*** Settings ***
Library     base.py
Library     runtime.py
Library     build.py
Library     OperatingSystem


*** Variables ***
${tmp_dir}              ${EMPTY}
${CLLTK_TRACING_PATH}    ${EMPTY}


*** Keywords ***
set up env
    ${tmp_dir}=    Create Temporary Directory
    Set Environment Variable    CLLTK_TRACING_PATH    ${tmp_dir.name}
    Set Test Variable    $CLLTK_TRACING_PATH    ${tmp_dir.name}
    Set Test Variable    $tmp_dir    ${tmp_dir}

clean up env
    Remove Temporary Directory    ${tmp_dir}
    Remove Environment Variable    CLLTK_TRACING_PATH

build and run ${content} for ${language}
    Write To main.gen.c    int main(){ return 0; }
    Write To main.gen.cpp    int main(){ return 0; }

    IF    '${language}' == 'C'
        Write To main.gen.c    ${content}
        ${output}=    Build Cmake Target robot_tests_exe_c    check_rc=True
        ${output}=    Run Cmake Target robot_tests_exe_c
        Should Be Equal As Integers    ${output.rc}    0    msg=${output.stderr}
    ELSE IF    '${language}' == 'CPP'
        Write To main.gen.cpp    ${content}
        ${output}=    Build Cmake Target robot_tests_exe_cpp    check_rc=True
        ${output}=    Run Cmake Target robot_tests_exe_cpp
        Should Be Equal As Integers    ${output.rc}    0    msg=${output.stderr}
    ELSE
        Fatal Error
    END

Clean Up Directory ${Path}

    @{items}=    List Files In Directory    ${Path}
    FOR    ${item}    IN    @{items}
        Remove File    ${Path}/${item}
    END

    @{subdirs}=    List Directories In Directory    ${Path}
    FOR    ${subdir}    IN    @{subdirs}
        Remove Directory    ${Path}/${subdir}
    END
