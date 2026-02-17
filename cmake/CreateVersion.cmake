

file(READ "${CMAKE_CURRENT_LIST_DIR}/../VERSION.md" VERSION_CONTENTS)
string(REGEX MATCH "^[0-9]+\\.[0-9]+\\.[0-9]+" CLLTK_VERSION_STRING "${VERSION_CONTENTS}")
string(REPLACE "." ";" VERSION_LIST ${CLLTK_VERSION_STRING})
list(GET VERSION_LIST 0 CLLTK_VERSION_MAJOR)
list(GET VERSION_LIST 1 CLLTK_VERSION_MINOR)
list(GET VERSION_LIST 2 CLLTK_VERSION_PATCH)
message(STATUS "CLLTK_VERSION_STRING: ${CLLTK_VERSION_STRING}")
message(STATUS "CLLTK_VERSION_MAJOR: ${CLLTK_VERSION_MAJOR}")
message(STATUS "CLLTK_VERSION_MINOR: ${CLLTK_VERSION_MINOR}")
message(STATUS "CLLTK_VERSION_PATCH: ${CLLTK_VERSION_PATCH}")


set(CLLTK_VERSION_INCLUDE_PATH ${CMAKE_CURRENT_BINARY_DIR}/generated/include/)
file(MAKE_DIRECTORY ${CLLTK_VERSION_INCLUDE_PATH}/CommonLowLevelTracingKit/)
add_custom_target(clltk-version-file
    COMMAND ${CMAKE_CURRENT_LIST_DIR}/gen_version_header.sh 
            -t ${CMAKE_CURRENT_LIST_DIR}/version.h.template 
            -o ${CLLTK_VERSION_INCLUDE_PATH}/CommonLowLevelTracingKit/version.gen.h
    BYPRODUCTS ${CLLTK_VERSION_INCLUDE_PATH}/CommonLowLevelTracingKit/version.gen.h
    DEPENDS 
        ${CMAKE_CURRENT_LIST_DIR}/gen_version_header.sh
        ${CMAKE_CURRENT_LIST_DIR}/version.h.template
        ${CMAKE_CURRENT_LIST_DIR}/../VERSION.md
)

add_library(clltk-version INTERFACE)
target_include_directories(clltk-version
    INTERFACE
        $<BUILD_INTERFACE:${CLLTK_VERSION_INCLUDE_PATH}>
)

add_dependencies(clltk-version clltk-version-file)