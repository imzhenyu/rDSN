
set(PROJ_NAME dsn.dev.utility)
project(${PROJ_NAME} C CXX)
include_directories(${CMAKE_SOURCE_DIR}/include)

set(PROJ_SRC "")
file(GLOB
        PROJ_SRC
        "${CMAKE_CURRENT_SOURCE_DIR}/*.cc"
        "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/*.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/*.hpp"
        )

add_library(${PROJ_NAME} "OBJECT" ${PROJ_SRC})
add_library(dsn.utility "STATIC" ${PROJ_SRC})

