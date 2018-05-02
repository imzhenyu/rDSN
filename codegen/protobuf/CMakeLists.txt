# Minimum CMake required
cmake_minimum_required(VERSION 2.8)

option(WITH_CSHARP "build csharp lib" OFF)

add_subdirectory(cmake)
if (WITH_CSHARP)
    add_subdirectory(csharp/src/Google.Protobuf)
endif()