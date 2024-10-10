*** Settings ***
Resource            ./../robot_resources/build.resource
Resource            ./../robot_resources/runtime.resource

Suite Setup         configure cmake and build default
Test Setup          set up env
Test Teardown       clean up env


*** Test Cases ***
Check build, compile and decoder for c
    ${content}=    catenate    SEPARATOR=\n
    ...    \#include "CommonLowLevelTracingKit/tracing.h"
    ...    CLLTK_TRACEBUFFER(BUFFER, 4096)
    ...    int main(void)
    ...    {
    ...    CLLTK_TRACEPOINT(BUFFER, "should be in tracebuffer for c");
    ...    return 0;
    ...    }
    build and run ${content} for C
    ${tracepoints}=    Decode Tracebuffer
    exist tracepoints    ${tracepoints}    7    clltk_decoder.py
    exist tracepoints    ${tracepoints}    1    should be in tracebuffer for c

Check build, compile and decoder for cpp
    ${content}=    catenate    SEPARATOR=\n
    ...    \#include "CommonLowLevelTracingKit/tracing.h"
    ...    CLLTK_TRACEBUFFER(BUFFER, 4096)
    ...    int main(void)
    ...    {
    ...    CLLTK_TRACEPOINT(BUFFER, "should be in tracebuffer for cpp");
    ...    return 0;
    ...    }
    build and run ${content} for CPP
    ${tracepoints}=    Decode Tracebuffer
    exist tracepoints    ${tracepoints}    7    clltk_decoder.py
    exist tracepoints    ${tracepoints}    1    should be in tracebuffer for cpp
