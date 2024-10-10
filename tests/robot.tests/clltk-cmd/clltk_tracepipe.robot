*** Settings ***
Resource            ./../robot_resources/build.resource
Resource            ./../robot_resources/runtime.resource
Library             Collections
Library             String

Suite Setup         configure cmake and build default
Test Setup          set up env
Test Teardown       clean up env



*** Test Cases ***

Subcommand tracepipe exist
    ${rc}=    CLLTK     tracepipe --help
    Should Be Equal As Integers    ${rc.rc}    0    msg=${rc.stderr}
    