set(PROJ_NAME dsn)
project(${PROJ_NAME} C CXX)
add_definitions(-DDSN_IN_CORE)

include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${CMAKE_SOURCE_DIR}/ext)

set(PROJ_SRC "")
file(GLOB
        PROJ_SRC
        "${CMAKE_CURRENT_SOURCE_DIR}/*.cc"
        "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/*.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/*.hpp"
        )

file(GLOB
        PROJ_SRC_TEST
        "${CMAKE_CURRENT_SOURCE_DIR}/*.test.cc"
        "${CMAKE_CURRENT_SOURCE_DIR}/*.test.cpp"
        )

if (DSN_STATIC)
    list(REMOVE_ITEM PROJ_SRC ${PROJ_SRC_TEST})
    add_library(${PROJ_NAME} "STATIC" ${PROJ_SRC}
        "${CMAKE_CURRENT_SOURCE_DIR}/all/plugins.cpp"
        $<TARGET_OBJECTS:dsn.dev.utility>     
        $<TARGET_OBJECTS:dsn.tools.common>
        $<TARGET_OBJECTS:dsn.tools.emulator>
        $<TARGET_OBJECTS:dsn.tools.http>
        )
    add_executable(${PROJ_NAME}.test ${PROJ_SRC_TEST}
        "${CMAKE_CURRENT_SOURCE_DIR}/all/test_main.cpp"
        $<TARGET_OBJECTS:dsn.tools.common.test>
        $<TARGET_OBJECTS:dsn.tools.emulator.test>
        $<TARGET_OBJECTS:dsn.tools.http.test>
        )
    target_link_libraries(${PROJ_NAME}.test dsn dsn.utility gtest)
else()
    add_library(${PROJ_NAME} "SHARED" ${PROJ_SRC})
    target_link_libraries(${PROJ_NAME} dsn.utility gtest)

    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}")
    add_executable(dsn.test "${CMAKE_CURRENT_SOURCE_DIR}/all/test_main.cpp")
    target_link_libraries(dsn.test ${PROJ_NAME} gtest gtest_main)
endif ()

if (UNIX)
    if((NOT APPLE))
        target_link_libraries(${PROJ_NAME} rt)
    endif ()
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        target_link_libraries(${PROJ_NAME} aio dl)
    endif()
    if((CMAKE_SYSTEM_NAME STREQUAL "FreeBSD"))
        target_link_libraries(${PROJ_NAME} util)
    endif()
    find_package (Threads)
    target_link_libraries(${PROJ_NAME} ${CMAKE_THREAD_LIBS_INIT})
endif ()

file(COPY test/ DESTINATION "${CMAKE_BINARY_DIR}/test/${PROJ_NAME}")

if (MSVC)
    add_test(
            NAME ${PROJ_NAME}.test
            COMMAND ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/Debug/dsn.test test.config.all.ini
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/test/${PROJ_NAME}
            )
else()
    add_test(
            NAME ${PROJ_NAME}.test
            #COMMAND ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/dsn.test test.config.all.ini
            COMMAND ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/dsn.test test.config.all.ini
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/test/${PROJ_NAME}
            )
endif()

