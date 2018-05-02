<?php
require_once($argv[1]); // type.php
require_once($argv[2]); // program.php
$file_prefix = $argv[3];
$top_dir = __DIR__."/../../..";
$libprotobuf = "protobuf";
if (strtoupper(substr(PHP_OS, 0, 3)) === 'WIN') 
{
    $top_dir = str_replace("\\", "/", $top_dir);
    $libprotobuf = "lib".$libprotobuf;
}
?>

cmake_minimum_required(VERSION 2.8.1)

project("<?=$_PROG->name?>" C CXX)

include("<?=$top_dir?>/bin/dsn.cmake")
ms_check_cxx11_support()
dsn_setup_compiler_flags()

include_directories(<?=$top_dir?>/include)
link_directories(<?=$top_dir?>/lib)

set(PROJ_SRC "")
file(GLOB
    PROJ_SRC
    "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.hpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/*.pb.*"
    )

set(PROJ_LIBS "")
if (UNIX)
    if((NOT APPLE))
        set(PROJ_LIBS ${PROJ_LIBS} rt)
    endif ()
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        set(PROJ_LIBS ${PROJ_LIBS} aio dl)
    endif()
    if((CMAKE_SYSTEM_NAME STREQUAL "FreeBSD"))
        set(PROJ_LIBS ${PROJ_LIBS} util)
    endif()
    find_package (Threads)
    set(PROJ_LIBS ${PROJ_LIBS} ${CMAKE_THREAD_LIBS_INIT})
endif ()

add_executable(<?=$_PROG->name?>.multiapp ${PROJ_SRC} ${CMAKE_CURRENT_SOURCE_DIR}/<?=$_PROG->name?>.multiapp.cpp)
target_link_libraries(<?=$_PROG->name?>.multiapp dsn <?=$libprotobuf?> ${PROJ_LIBS})

add_executable(<?=$_PROG->name?>.server ${PROJ_SRC} ${CMAKE_CURRENT_SOURCE_DIR}/<?=$_PROG->name?>.server.cpp)
target_link_libraries(<?=$_PROG->name?>.server dsn <?=$libprotobuf?> ${PROJ_LIBS})

add_executable(<?=$_PROG->name?>.client ${PROJ_SRC} ${CMAKE_CURRENT_SOURCE_DIR}/<?=$_PROG->name?>.client.cpp)
target_link_libraries(<?=$_PROG->name?>.client dsn <?=$libprotobuf?> ${PROJ_LIBS})

file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/<?=$_PROG->name?>.client.config.ini" DESTINATION "${CMAKE_BINARY_DIR}/bin")
file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/<?=$_PROG->name?>.server.config.ini" DESTINATION "${CMAKE_BINARY_DIR}/bin")
file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/<?=$_PROG->name?>.multiapp.config.ini" DESTINATION "${CMAKE_BINARY_DIR}/bin")
