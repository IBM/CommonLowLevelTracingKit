
set(CLLTK_COMPILER "unknown")
if (${CMAKE_C_COMPILER_ID} MATCHES "GNU" AND ${CMAKE_CXX_COMPILER_ID} MATCHES "GNU")
    set(CLLTK_COMPILER "GNU")
    # check gcc version
    execute_process(COMMAND ${CMAKE_C_COMPILER} -dumpfullversion OUTPUT_VARIABLE C_COMPILER_VERSION)
    if (C_COMPILER_VERSION AND C_COMPILER_VERSION VERSION_LESS 10.0)
        message(FATAL_ERROR "GCC version must be at least 10.0! and your version for " ${CMAKE_C_COMPILER}  " is " ${C_COMPILER_VERSION})
    endif()
    # check g++ version
    execute_process(COMMAND ${CMAKE_CXX_COMPILER} -dumpfullversion OUTPUT_VARIABLE CXX_COMPILER_VERSION)
    if (CXX_COMPILER_VERSION AND CXX_COMPILER_VERSION VERSION_LESS 10.0)
        message(FATAL_ERROR "G++ version must be at least 10.0! and your version for " ${CMAKE_CXX_COMPILER}  " is " ${CXX_COMPILER_VERSION})
    endif()

elseif (${CMAKE_C_COMPILER_ID} MATCHES "Clang" AND ${CMAKE_CXX_COMPILER_ID} MATCHES "Clang")
    set(CLLTK_COMPILER "Clang")
else()
    message(FATAL_ERROR "unsupported compiler with CMAKE_C_COMPILER_ID=${CMAKE_C_COMPILER_ID} and CMAKE_CXX_COMPILER_ID=${CMAKE_CXX_COMPILER_ID}")
endif()
