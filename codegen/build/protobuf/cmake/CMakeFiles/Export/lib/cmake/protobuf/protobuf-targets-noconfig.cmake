#----------------------------------------------------------------
# Generated CMake target import file.
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "libprotobuf-lite" for configuration ""
set_property(TARGET libprotobuf-lite APPEND PROPERTY IMPORTED_CONFIGURATIONS NOCONFIG)
set_target_properties(libprotobuf-lite PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_NOCONFIG "CXX"
  IMPORTED_LOCATION_NOCONFIG "${_IMPORT_PREFIX}/lib/libprotobuf-lite.a"
  )

list(APPEND _IMPORT_CHECK_TARGETS libprotobuf-lite )
list(APPEND _IMPORT_CHECK_FILES_FOR_libprotobuf-lite "${_IMPORT_PREFIX}/lib/libprotobuf-lite.a" )

# Import target "libprotobuf" for configuration ""
set_property(TARGET libprotobuf APPEND PROPERTY IMPORTED_CONFIGURATIONS NOCONFIG)
set_target_properties(libprotobuf PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_NOCONFIG "CXX"
  IMPORTED_LINK_INTERFACE_LIBRARIES_NOCONFIG "ZLIB::ZLIB"
  IMPORTED_LOCATION_NOCONFIG "${_IMPORT_PREFIX}/lib/libprotobuf.a"
  )

list(APPEND _IMPORT_CHECK_TARGETS libprotobuf )
list(APPEND _IMPORT_CHECK_FILES_FOR_libprotobuf "${_IMPORT_PREFIX}/lib/libprotobuf.a" )

# Import target "libprotoc" for configuration ""
set_property(TARGET libprotoc APPEND PROPERTY IMPORTED_CONFIGURATIONS NOCONFIG)
set_target_properties(libprotoc PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_NOCONFIG "CXX"
  IMPORTED_LINK_INTERFACE_LIBRARIES_NOCONFIG "libprotobuf"
  IMPORTED_LOCATION_NOCONFIG "${_IMPORT_PREFIX}/lib/libprotoc.a"
  )

list(APPEND _IMPORT_CHECK_TARGETS libprotoc )
list(APPEND _IMPORT_CHECK_FILES_FOR_libprotoc "${_IMPORT_PREFIX}/lib/libprotoc.a" )

# Import target "protoc" for configuration ""
set_property(TARGET protoc APPEND PROPERTY IMPORTED_CONFIGURATIONS NOCONFIG)
set_target_properties(protoc PROPERTIES
  IMPORTED_LOCATION_NOCONFIG "${_IMPORT_PREFIX}/bin/protoc"
  )

list(APPEND _IMPORT_CHECK_TARGETS protoc )
list(APPEND _IMPORT_CHECK_FILES_FOR_protoc "${_IMPORT_PREFIX}/bin/protoc" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
