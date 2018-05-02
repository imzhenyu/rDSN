cmake_minimum_required(VERSION 2.8.7)
project(dsn C CXX)
include(../bin/dsn.cmake)

dsn_common_setup()

# protobuf
set(protobuf_BUILD_TESTS OFF CACHE BOOL "disable protbuf test")
add_subdirectory(protobuf/cmake)
