
include(ProcessorCount)
ProcessorCount(ProcessorCounter)
if(NOT ProcessorCounter EQUAL 0 AND NOT DEFINED CMAKE_BUILD_PARALLEL_LEVEL)
    set(CMAKE_BUILD_PARALLEL_LEVEL ${ProcessorCounter})
    set(CTEST_BUILD_FLAGS -j${ProcessorCounter})
    set(ctest_test_args ${ctest_test_args} PARALLEL_LEVEL ${ProcessorCounter})
    set(CPACK_THREADS ${ProcessorCounter})
endif()
message(STATUS "Setting multiple processor (${CMAKE_BUILD_PARALLEL_LEVEL})")

find_program(CCACHE ccache)
if(CCACHE)
    message(STATUS "Setting ccache (${CCACHE})")
    set(CMAKE_C_COMPILER_LAUNCHER "${CCACHE}")
    set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE}")
endif()

set(CMAKE_POSITION_INDEPENDENT_CODE ON)
