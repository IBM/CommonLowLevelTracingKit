
add_library(clltk_tracing_abstraction_interface INTERFACE)
target_include_directories(clltk_tracing_abstraction_interface
    INTERFACE
        include
)

if(UNIX)
    add_library(clltk_tracing_abstraction STATIC
        linux_user_space/file.c
        linux_user_space/sync.c
        linux_user_space/info.c
        linux_user_space/error.c
        linux_user_space/memory.c
    )
    target_link_libraries(clltk_tracing_abstraction
        PRIVATE
            pthread
    )
    set_target_properties(clltk_tracing_abstraction
        PROPERTIES
            COMPILE_LANGUAGE C
            C_STANDARD 17
            C_EXTENSIONS ON
            C_STANDARD_REQUIRED OFF
    )
else()
    message(FATAL_ERROR "unsupported target: ${CMAKE_SYSTEM_NAME}")
endif()

target_link_libraries(clltk_tracing_abstraction
    PUBLIC
        clltk_tracing_abstraction_interface
)
set_target_properties(clltk_tracing_abstraction
    PROPERTIES
        COMPILE_WARNING_AS_ERROR ON
)

target_compile_options(clltk_tracing_abstraction
    PUBLIC
        ${clltk_public_compiler_flags}
    PRIVATE
        ${clltk_private_compiler_flags}
)