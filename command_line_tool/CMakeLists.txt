# Copyright (c) 2024, International Business Machines
# SPDX-License-Identifier: BSD-2-Clause-Patent

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(interface)

set(CLLTK_COMMAND_LINE_COMMANDS "") # each command will add it self to this list.
# So disabling a command must happens in its own cmake file

# iterate over commands 
file(GLOB COMMAND_FOLDERS ${CMAKE_CURRENT_SOURCE_DIR}/commands/*)
foreach(COMMAND_FOLDER IN LISTS COMMAND_FOLDERS)
    if(IS_DIRECTORY ${COMMAND_FOLDER} AND EXISTS ${COMMAND_FOLDER}/CMakeLists.txt)
        add_subdirectory(${COMMAND_FOLDER})
    endif()
endforeach()


# create the clltk executable 
add_executable(clltk-cmd
    main/main.cpp
    main/validators.cpp
)

target_link_libraries(clltk-cmd
    PRIVATE
        clltk-cmd-interface
        clltk-version
)

# Check that all commands are OBJECT or SHARED libraries
foreach(command_target ${CLLTK_COMMAND_LINE_COMMANDS})
    get_target_property(target_type ${command_target} TYPE)
    if(NOT "${target_type}" STREQUAL "OBJECT_LIBRARY")
        message(FATAL_ERROR "To ensure that commands in the main function are detected at runtime, ${command_target} must be OBJECT library, not ${target_type}.")
    endif()
    target_sources(clltk-cmd 
        PRIVATE
            $<TARGET_OBJECTS:${command_target}>
    )

    target_link_libraries(clltk-cmd
        PRIVATE
            ${command_target}
    )
endforeach()


set_target_properties(clltk-cmd PROPERTIES 
    OUTPUT_NAME clltk
    INTERPROCEDURAL_OPTIMIZATION TRUE
)

target_compile_options(clltk-cmd
    PRIVATE
        -Os
)
install(TARGETS
    clltk-cmd
    ${CLLTK_COMMAND_LINE_COMMANDS}
    EXPORT CLLTKTracingTargets
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT cmd
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT cmd
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT cmd
)
