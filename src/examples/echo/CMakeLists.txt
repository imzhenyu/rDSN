
set(PROJ_NAME echo)
project(${PROJ_NAME} C CXX)
include_directories(${CMAKE_SOURCE_DIR}/include)

set(PROJ_SRC "")
file(GLOB
    PROJ_SRC
    "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.hpp"
    )

add_executable(${PROJ_NAME}.multiapp ${PROJ_SRC} ${CMAKE_CURRENT_SOURCE_DIR}/echo.multiapp.cpp)
target_link_libraries(${PROJ_NAME}.multiapp dsn)

add_executable(${PROJ_NAME}.server ${PROJ_SRC} ${CMAKE_CURRENT_SOURCE_DIR}/echo.server.cpp)
target_link_libraries(${PROJ_NAME}.server dsn)

add_executable(${PROJ_NAME}.client ${PROJ_SRC} ${CMAKE_CURRENT_SOURCE_DIR}/echo.client.cpp)
target_link_libraries(${PROJ_NAME}.client dsn)

file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/echo.client.config.ini" DESTINATION "${CMAKE_BINARY_DIR}/bin")
file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/echo.server.config.ini" DESTINATION "${CMAKE_BINARY_DIR}/bin")
file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/echo.multiapp.config.ini" DESTINATION "${CMAKE_BINARY_DIR}/bin")
