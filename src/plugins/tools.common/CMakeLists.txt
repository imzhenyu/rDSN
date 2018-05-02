
set(PROJ_NAME dsn.tools.common)
project(${PROJ_NAME} C CXX)
include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${GTEST_INCLUDE_DIR})

set(PROJ_SRC "")
file(GLOB
        PROJ_SRC
        ${PROJ_SRC}
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
    add_library(${PROJ_NAME} "OBJECT" ${PROJ_SRC})
    add_library(${PROJ_NAME}.test "OBJECT" ${PROJ_SRC_TEST})
else ()
    add_library(${PROJ_NAME} "SHARED" ${PROJ_SRC})
    target_link_libraries(${PROJ_NAME} dsn gtest)
endif ()

file(COPY test/ DESTINATION "${CMAKE_BINARY_DIR}/test/${PROJ_NAME}")

if (MSVC)
    add_test(
            NAME ${PROJ_NAME}.test
            COMMAND ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/Debug/dsn.test test.config.tools.common.ini
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/test/${PROJ_NAME}
            )
else()
    add_test(
            NAME ${PROJ_NAME}.test
            COMMAND ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/dsn.test test.config.tools.common.ini
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/test/${PROJ_NAME}
            )
endif()