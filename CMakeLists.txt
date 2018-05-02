cmake_minimum_required(VERSION 2.8.7)
project(dsn C CXX)
include(bin/dsn.cmake)

dsn_common_setup()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin CACHE STRING "" FORCE)
if (WIN32)
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin CACHE STRING "" FORCE)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin CACHE STRING "" FORCE)
else ()
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib CACHE STRING "" FORCE)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib CACHE STRING "" FORCE)
endif ()

install(DIRECTORY ${CMAKE_SOURCE_DIR}/include/ DESTINATION include)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/bin/ DESTINATION bin USE_SOURCE_PERMISSIONS)
if (WIN32)
    install(DIRECTORY ${CMAKE_BINARY_DIR}/bin/ DESTINATION lib USE_SOURCE_PERMISSIONS)
else()
    install(DIRECTORY ${CMAKE_BINARY_DIR}/lib/ DESTINATION lib)
    install(PROGRAMS ${CMAKE_BINARY_DIR}/bin/dsn.svchost DESTINATION bin)
    install(PROGRAMS ${CMAKE_BINARY_DIR}/bin/dsn.cli DESTINATION bin)
endif()

if(DEFINED WARNING_ALL)
    #add_compile_options("-Wall")
    SET(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Wall")
    SET(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -Wall")
endif()

OPTION(ENABLE_GCOV "Enable gcov (debug, Linux builds only)" OFF)
if(ENABLE_GCOV AND UNIX)
    #add_compile_options("-fprofile-arcs" "-ftest-coverage")
    SET(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fprofile-arcs -ftest-coverage -DENABLE_GCOV")
    SET(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -fprofile-arcs -ftest-coverage -DENABLE_GCOV")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fprofile-arcs -ftest-coverage -lgcov")
endif()

######################
# external projects
######################

# gtest
include_directories(${CMAKE_SOURCE_DIR}/ext/googletest/include)
set(gtest_force_shared_crt OFF CACHE BOOL "disable gtest shared crt test")
#set(BUILD_SHARED_LIBS_SAVE "${BUILD_SHARED_LIBS}")
#set(BUILD_SHARED_LIBS ON)
add_subdirectory(ext/googletest)
#set(BUILD_SHARED_LIBS "${BUILD_SHARED_LIBS_SAVE}")

# concurrentqueue
include_directories(${CMAKE_SOURCE_DIR}/ext/concurrentqueue)

######################
# dsn projects
######################

dsn_add_pseudo_projects()
enable_testing()
add_subdirectory(src)

