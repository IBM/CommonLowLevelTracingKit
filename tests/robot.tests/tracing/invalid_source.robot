*** Settings ***
Resource        ./../robot_resources/build.resource

Suite Setup     configure cmake and build default


*** Test Cases ***
detect build failes
    ${content}=    catenate    SEPARATOR=\n
    ...    \#error \"error\"
    ...    int main(void){return 0;}
    ${error}=    Set Variable    \#error "error"
    source ${content} for C failes with    ${error}
    source ${content} for CPP failes with    ${error}

twice same buffer definition
    ${content}=    catenate    SEPARATOR=\n
    ...    \#include "CommonLowLevelTracingKit/tracing/tracing.h"
    ...    CLLTK_TRACEBUFFER(BUFFER, 4096)
    ...    CLLTK_TRACEBUFFER(BUFFER, 4096)
    ...    int main(void){}
    source ${content} for C failes with    redefinition of
    source ${content} for CPP failes with    redefinition of

missing buffer size
    ${content}=    catenate    SEPARATOR=\n
    ...    \#include "CommonLowLevelTracingKit/tracing/tracing.h"
    ...    CLLTK_TRACEBUFFER(BUFFER)
    ...    int main(void){}
    ${error}=    Set Variable    macro "CLLTK_TRACEBUFFER" requires 2 arguments, but only 1 given
    source ${content} for C failes with    ${error}
    source ${content} for CPP failes with    ${error}

tracebuffer name as a string
    ${content}=    catenate    SEPARATOR=\n
    ...    \#include "CommonLowLevelTracingKit/tracing/tracing.h"
    ...    CLLTK_TRACEBUFFER("BUFFER", 1024)
    ...    int main(void){}
    ${error}=    Set Variable    pasting "_clltk_" and ""BUFFER"" does not give a valid preprocessing token
    source ${content} for C failes with    ${error}
    source ${content} for CPP failes with    ${error}

missing buffer definition
    ${content}=    catenate    SEPARATOR=\n
    ...    \#include "CommonLowLevelTracingKit/tracing/tracing.h"
    ...    int main(void){CLLTK_TRACEPOINT(BUFFER, "should be in tracebuffer for c");}
    source ${content} for C failes with    ‘_clltk_BUFFER’ undeclared
    source ${content} for CPP failes with    ‘_clltk_BUFFER’ was not declared in this scope

11 arguments
    ${content}=    catenate    SEPARATOR=\n
    ...    \#include "CommonLowLevelTracingKit/tracing/tracing.h"
    ...    CLLTK_TRACEBUFFER(BUFFER, 1024)
    ...    int main(void){
    ...    CLLTK_TRACEPOINT(BUFFER,"0%u 1%u 2%u 3%u 4%u 5%u 6%u 7%u 8%u 9%u 10%u",
    ...    0,1,2,3,4,5,6,7,8,9,10);
    ...    }
    ${error}=    Set Variable    only supporting up to 10 arguments
    source ${content} for C failes with    ${error}
    source ${content} for CPP failes with    ${error}

20 arguments
    ${content}=    catenate    SEPARATOR=\n
    ...    \#include "CommonLowLevelTracingKit/tracing/tracing.h"
    ...    CLLTK_TRACEBUFFER(BUFFER, 1024)
    ...    int main(void){
    ...    CLLTK_TRACEPOINT(BUFFER,
    ...    "00%u 01%u 02%u 03%u 04%u 05%u 06%u 07%u 08%u 09%u "
    ...    "10%u 11%u 12%u 13%u 14%u 15%u 16%u 17%u 18%u 19%u ",
    ...    0, 1, 2, 3, 4, 5, 6, 7, 8, 9,
    ...    10,11,12,13,14,15,16,17,18,19
    ...    );
    ...    }
    ${error}=    Set Variable    only supporting up to 10 arguments
    source ${content} for C failes with    ${error}
    source ${content} for CPP failes with    ${error}

40 arguments
    ${content}=    catenate    SEPARATOR=\n
    ...    \#include "CommonLowLevelTracingKit/tracing/tracing.h"
    ...    CLLTK_TRACEBUFFER(BUFFER, 1024)
    ...    int main(void){
    ...    CLLTK_TRACEPOINT(BUFFER,
    ...    "00%u 01%u 02%u 03%u 04%u 05%u 06%u 07%u 08%u 09%u "
    ...    "10%u 11%u 12%u 13%u 14%u 15%u 16%u 17%u 18%u 19%u "
    ...    "20%u 21%u 22%u 23%u 24%u 25%u 26%u 27%u 28%u 29%u "
    ...    "30%u 31%u 32%u 33%u 34%u 35%u 36%u 37%u 38%u 39%u ",
    ...    0, 1, 2, 3, 4, 5, 6, 7, 8, 9,
    ...    10,11,12,13,14,15,16,17,18,19,
    ...    20,21,22,23,24,25,26,27,28,29,
    ...    30,31,32,33,34,35,36,37,38,39
    ...    );
    ...    }
    ${error}=    Set Variable    only supporting up to 10 arguments
    source ${content} for C failes with    ${error}
    source ${content} for CPP failes with    ${error}

more arguments than formats
    ${content}=    catenate    SEPARATOR=\n
    ...    \#include "CommonLowLevelTracingKit/tracing/tracing.h"
    ...    CLLTK_TRACEBUFFER(BUFFER, 1024)
    ...    int main(void){
    ...    CLLTK_TRACEPOINT(BUFFER,"0%u 1%u 2%u 3%u 4%u 5%u",
    ...    0,1,2,3,4,5,6);
    ...    }
    ${error}=    Set Variable    -Werror=format-extra-args
    source ${content} for C failes with    ${error}
    source ${content} for CPP failes with    ${error}

more formats than arguments
    ${content}=    catenate    SEPARATOR=\n
    ...    \#include "CommonLowLevelTracingKit/tracing/tracing.h"
    ...    CLLTK_TRACEBUFFER(BUFFER, 1024)
    ...    int main(void){
    ...    CLLTK_TRACEPOINT(BUFFER,"0%u 1%u 2%u 3%u 4%u 5%u",
    ...    0,1,2,3,4);
    ...    }
    ${error}=    Set Variable    -Werror=format=
    source ${content} for C failes with    ${error}
    source ${content} for CPP failes with    ${error}

tracepoint in none-static cpp inline function not possible due to gcc deficiencies
    ${content}=    catenate    SEPARATOR=\n
    ...    \#include "CommonLowLevelTracingKit/tracing/tracing.h"
    ...    CLLTK_TRACEBUFFER(BUFFER, 1024)
    ...    inline void foo(void){
    ...        CLLTK_TRACEPOINT(BUFFER, "from inline");
    ...    }
    ...    int main(void){
    ...        foo();
    ...        CLLTK_TRACEPOINT(BUFFER, "from main");
    ...    }
    ${error}=    Set Variable   error: ‘_meta’ causes a section type conflict with 
    source ${content} for CPP failes with    ${error}
