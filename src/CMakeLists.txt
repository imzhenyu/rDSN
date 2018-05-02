
if (DSN_STATIC)
    add_definitions(-DDSN_STATIC)
endif ()

add_subdirectory(core)
add_subdirectory(utility)
add_subdirectory(tools)
add_subdirectory(plugins)
add_subdirectory(examples)
