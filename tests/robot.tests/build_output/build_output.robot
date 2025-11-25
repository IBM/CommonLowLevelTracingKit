*** Settings ***
Resource            ./../robot_resources/build.resource
Resource            ./../robot_resources/runtime.resource
Library             Process
Library             String

Suite Setup         configure cmake and build default
Test Setup          set up env
Test Teardown       clean up env

*** Test Cases ***
test static tracing library
    Build Cmake Target clltk_tracing_static
    ${build_folder}=     Get Build Folder
    ${static_lib}=       Set Variable    ${build_folder}/tracing_library/libclltk_tracing_static.a
    File Should Exist    path=${static_lib}    msg=static tracing library is missing
    ${output}=           Run Process  ar  -xv  --output\=${tmp_dir.name}  ${static_lib}
    Should Be Equal As Integers    ${output.rc}    0    msg=could not open static lib

    ${lines}=            Split To Lines    ${output.stdout}
    FOR    ${line}    IN    @{lines}
        ${line}=      Split String    ${line}
        ${file}=      Set Variable    ${line[2]}
        ${output}=    Run Process  file    ${file}
        Should Be Equal As Integers    ${output.rc}    0    msg=file command failed for (${file}) in (${static_lib})
        Should Contain    ${output.stdout}    relocatable   msg=not relocatable (${file}) in (${static_lib})
    END

test shared tracing library
    Build Cmake Target clltk_tracing_shared
    ${build_folder}=     Get Build Folder
    ${shared_lib}=       Set Variable    ${build_folder}/tracing_library/libclltk_tracing.so
    File Should Exist    path=${shared_lib}    msg=shared tracing library is missing
    ${output}=           Run Process    readelf -d  ${shared_lib}    shell=true
    Should Not Contain    ${output.stdout}    TEXTREL    msg=${shared_lib} is not PIC (contains TEXTREL)

test static snapshot library
    Build Cmake Target clltk_snapshot_static
    ${build_folder}=     Get Build Folder
    ${static_lib}=       Set Variable    ${build_folder}/snapshot_library/libclltk_snapshot_static.a
    File Should Exist    path=${static_lib}    msg=static snapshot library is missing
    ${output}=           Run Process  ar  -xv  --output\=${tmp_dir.name}  ${static_lib}
    Should Be Equal As Integers    ${output.rc}    0    msg=could not open static lib

    ${lines}=            Split To Lines    ${output.stdout}
    FOR    ${line}    IN    @{lines}
        ${line}=      Split String    ${line}
        ${file}=      Set Variable    ${line[2]}
        ${output}=    Run Process  file    ${file}
        Should Be Equal As Integers    ${output.rc}    0    msg=file command failed for (${file}) in (${static_lib})
        Should Contain    ${output.stdout}    relocatable   msg=not relocatable (${file}) in (${static_lib})
    END

test shared snapshot library
    Build Cmake Target clltk_snapshot_shared
    ${build_folder}=     Get Build Folder
    ${shared_lib}=       Set Variable    ${build_folder}/snapshot_library/libclltk_snapshot.so
    File Should Exist    path=${shared_lib}    msg=shared snapshot library is missing
    ${output}=           Run Process    readelf -d  ${shared_lib}    shell=true
    Should Not Contain    ${output.stdout}    TEXTREL    msg=${shared_lib} is not PIC (contains TEXTREL)