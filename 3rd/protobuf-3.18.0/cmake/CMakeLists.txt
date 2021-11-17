# Minimum CMake required
cmake_minimum_required(VERSION 3.1.3)

if(protobuf_VERBOSE)
  message(STATUS "Protocol Buffers Configuring...")
endif()

# CMake policies
cmake_policy(SET CMP0022 NEW)
# On MacOS use @rpath/ for target's install name prefix path
if (POLICY CMP0042)
  cmake_policy(SET CMP0042 NEW)
endif ()
# Clear VERSION variables when no VERSION is given to project()
if(POLICY CMP0048)
  cmake_policy(SET CMP0048 NEW)
endif()

# Project
project(protobuf C CXX)

# Add c++11 flags
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# The Intel compiler isn't able to deal with noinline member functions of
# template classes defined in headers.  As such it spams the output with
#   warning #2196: routine is both "inline" and "noinline"
# This silences that warning.
if (CMAKE_CXX_COMPILER_ID MATCHES Intel)
  string(APPEND CMAKE_CXX_FLAGS " -diag-disable=2196")
endif()

# Options
if(WITH_PROTOC)
  set(protobuf_PROTOC_EXE ${WITH_PROTOC} CACHE FILEPATH "Protocol Buffer Compiler executable" FORCE)
endif()
option(protobuf_BUILD_PROTOC_BINARIES "Build libprotoc and protoc compiler" ON)
option(protobuf_BUILD_LIBPROTOC "Build libprotoc" ON)
option(protobuf_DISABLE_RTTI "Remove runtime type information in the binaries" OFF)

if (BUILD_SHARED_LIBS)
  set(protobuf_BUILD_SHARED_LIBS_DEFAULT ON)
else (BUILD_SHARED_LIBS)
  set(protobuf_BUILD_SHARED_LIBS_DEFAULT OFF)
endif (BUILD_SHARED_LIBS)

option(protobuf_BUILD_SHARED_LIBS "Build Shared Libraries" ${protobuf_BUILD_SHARED_LIBS_DEFAULT})

include(CMakeDependentOption)
cmake_dependent_option(protobuf_MSVC_STATIC_RUNTIME "Link static runtime libraries" ON "NOT protobuf_BUILD_SHARED_LIBS" OFF)

set(protobuf_WITH_ZLIB_DEFAULT ON)
option(protobuf_WITH_ZLIB "Build with zlib support" ${protobuf_WITH_ZLIB_DEFAULT})

set(protobuf_DEBUG_POSTFIX "d" CACHE STRING "Default debug postfix")
mark_as_advanced(protobuf_DEBUG_POSTFIX)

# User options
include(protobuf-options.cmake)

# Overrides for option dependencies
if (protobuf_BUILD_PROTOC_BINARIES)
  set(protobuf_BUILD_LIBPROTOC ON)
endif ()

# Path to main configure script
set(protobuf_CONFIGURE_SCRIPT "../configure.ac")

# Parse configure script
set(protobuf_AC_INIT_REGEX
  "^AC_INIT\\(\\[([^]]+)\\],\\[([^]]+)\\],\\[([^]]+)\\],\\[([^]]+)\\]\\)$")
file(STRINGS "${protobuf_CONFIGURE_SCRIPT}" protobuf_AC_INIT_LINE
  LIMIT_COUNT 1 REGEX "^AC_INIT")
# Description
string(REGEX REPLACE        "${protobuf_AC_INIT_REGEX}" "\\1"
    protobuf_DESCRIPTION    "${protobuf_AC_INIT_LINE}")
# Version
string(REGEX REPLACE        "${protobuf_AC_INIT_REGEX}" "\\2"
    protobuf_VERSION_STRING "${protobuf_AC_INIT_LINE}")
# Contact
string(REGEX REPLACE        "${protobuf_AC_INIT_REGEX}" "\\3"
    protobuf_CONTACT        "${protobuf_AC_INIT_LINE}")
# Parse version tweaks
set(protobuf_VERSION_REGEX "^([0-9]+)\\.([0-9]+)\\.([0-9]+)([-]rc[-]|\\.)?([0-9]*)$")
string(REGEX REPLACE     "${protobuf_VERSION_REGEX}" "\\1"
  protobuf_VERSION_MAJOR "${protobuf_VERSION_STRING}")
string(REGEX REPLACE     "${protobuf_VERSION_REGEX}" "\\2"
  protobuf_VERSION_MINOR "${protobuf_VERSION_STRING}")
string(REGEX REPLACE     "${protobuf_VERSION_REGEX}" "\\3"
  protobuf_VERSION_PATCH "${protobuf_VERSION_STRING}")
string(REGEX REPLACE     "${protobuf_VERSION_REGEX}" "\\5"
  protobuf_VERSION_PRERELEASE "${protobuf_VERSION_STRING}")

message(STATUS "${protobuf_VERSION_PRERELEASE}")

# Package version
set(protobuf_VERSION
  "${protobuf_VERSION_MAJOR}.${protobuf_VERSION_MINOR}.${protobuf_VERSION_PATCH}")

if(protobuf_VERSION_PRERELEASE)
  set(protobuf_VERSION "${protobuf_VERSION}.${protobuf_VERSION_PRERELEASE}")
else()
  set(protobuf_VERSION "${protobuf_VERSION}.0")
endif()
message(STATUS "${protobuf_VERSION}")

if(protobuf_VERBOSE)
  message(STATUS "Configuration script parsing status [")
  message(STATUS "  Description : ${protobuf_DESCRIPTION}")
  message(STATUS "  Version     : ${protobuf_VERSION} (${protobuf_VERSION_STRING})")
  message(STATUS "  Contact     : ${protobuf_CONTACT}")
  message(STATUS "]")
endif()

add_definitions(-DGOOGLE_PROTOBUF_CMAKE_BUILD)

if (protobuf_DISABLE_RTTI)
  add_definitions(-DGOOGLE_PROTOBUF_NO_RTTI=1)
endif()

find_package(Threads REQUIRED)
if (CMAKE_USE_PTHREADS_INIT)
  add_definitions(-DHAVE_PTHREAD)
endif (CMAKE_USE_PTHREADS_INIT)

set(_protobuf_FIND_ZLIB)
if (protobuf_WITH_ZLIB)
  find_package(ZLIB)
  if (ZLIB_FOUND)
    set(HAVE_ZLIB 1)
    # FindZLIB module define ZLIB_INCLUDE_DIRS variable
    # Set ZLIB_INCLUDE_DIRECTORIES for compatible
    set(ZLIB_INCLUDE_DIRECTORIES ${ZLIB_INCLUDE_DIRECTORIES} ${ZLIB_INCLUDE_DIRS})
    # Using imported target if exists
    if (TARGET ZLIB::ZLIB)
      set(ZLIB_LIBRARIES ZLIB::ZLIB)
      set(_protobuf_FIND_ZLIB "if(NOT ZLIB_FOUND)\n  find_package(ZLIB)\nendif()")
    endif (TARGET ZLIB::ZLIB)
  else (ZLIB_FOUND)
    set(HAVE_ZLIB 0)
    # Explicitly set these to empty (override NOT_FOUND) so cmake doesn't
    # complain when we use them later.
    set(ZLIB_INCLUDE_DIRECTORIES)
    set(ZLIB_LIBRARIES)
  endif (ZLIB_FOUND)
endif (protobuf_WITH_ZLIB)

if (HAVE_ZLIB)
  add_definitions(-DHAVE_ZLIB)
endif (HAVE_ZLIB)

# We need to link with libatomic on systems that do not have builtin atomics, or
# don't have builtin support for 8 byte atomics
set(protobuf_LINK_LIBATOMIC false)
if (NOT MSVC)
  include(CheckCXXSourceCompiles)
  set(OLD_CMAKE_REQUIRED_FLAGS ${CMAKE_REQUIRED_FLAGS})
  set(CMAKE_REQUIRED_FLAGS ${CMAKE_REQUIRED_FLAGS} -std=c++11)
  check_cxx_source_compiles("
    #include <atomic>
    int main() {
      return std::atomic<int64_t>{};
    }
  " protobuf_HAVE_BUILTIN_ATOMICS)
  if (NOT protobuf_HAVE_BUILTIN_ATOMICS)
    set(protobuf_LINK_LIBATOMIC true)
  endif (NOT protobuf_HAVE_BUILTIN_ATOMICS)
  set(CMAKE_REQUIRED_FLAGS ${OLD_CMAKE_REQUIRED_FLAGS})
endif (NOT MSVC)

if (protobuf_BUILD_SHARED_LIBS)
  set(protobuf_SHARED_OR_STATIC "SHARED")
else (protobuf_BUILD_SHARED_LIBS)
  set(protobuf_SHARED_OR_STATIC "STATIC")
  # In case we are building static libraries, link also the runtime library statically
  # so that MSVCR*.DLL is not required at runtime.
  # https://msdn.microsoft.com/en-us/library/2kzt1wy3.aspx
  # This is achieved by replacing msvc option /MD with /MT and /MDd with /MTd
  # http://www.cmake.org/Wiki/CMake_FAQ#How_can_I_build_my_MSVC_application_with_a_static_runtime.3F
  if (MSVC AND protobuf_MSVC_STATIC_RUNTIME)
    foreach(flag_var
        CMAKE_CXX_FLAGS CMAKE_CXX_FLAGS_DEBUG CMAKE_CXX_FLAGS_RELEASE
        CMAKE_CXX_FLAGS_MINSIZEREL CMAKE_CXX_FLAGS_RELWITHDEBINFO)
      if(${flag_var} MATCHES "/MD")
        string(REGEX REPLACE "/MD" "/MT" ${flag_var} "${${flag_var}}")
      endif(${flag_var} MATCHES "/MD")
    endforeach(flag_var)
  endif (MSVC AND protobuf_MSVC_STATIC_RUNTIME)
endif (protobuf_BUILD_SHARED_LIBS)

get_filename_component(protobuf_source_dir ${protobuf_SOURCE_DIR} PATH)

include_directories(
  ${ZLIB_INCLUDE_DIRECTORIES}
  ${protobuf_BINARY_DIR}
  ${protobuf_source_dir}/src)

if (MSVC)
  # Add the "lib" prefix for generated .lib outputs.
  set(LIB_PREFIX lib)
else (MSVC)
  # When building with "make", "lib" prefix will be added automatically by
  # the build tool.
  set(LIB_PREFIX)
endif (MSVC)

if (protobuf_UNICODE)
  add_definitions(-DUNICODE -D_UNICODE)
endif (protobuf_UNICODE)

SET(LIBRARY_OUTPUT_PATH ${CMAKE_SOURCE_DIR}/libs)
SET(EXECUTABLE_OUTPUT_PATH ${CMAKE_SOURCE_DIR}/libs)

include(libprotobuf-lite.cmake)
include(libprotobuf.cmake)
if (protobuf_BUILD_LIBPROTOC)
  include(libprotoc.cmake)
endif (protobuf_BUILD_LIBPROTOC)
if (protobuf_BUILD_PROTOC_BINARIES)
  include(protoc.cmake)
  if (NOT DEFINED protobuf_PROTOC_EXE)
    set(protobuf_PROTOC_EXE protoc)
  endif (NOT DEFINED protobuf_PROTOC_EXE)
endif (protobuf_BUILD_PROTOC_BINARIES)

include(install.cmake)

if(protobuf_VERBOSE)
  message(STATUS "Protocol Buffers Configuring done")
endif(protobuf_VERBOSE)
