# 3.5 is actually available almost everywhere, but this a good minimum
cmake_minimum_required(VERSION 3.4)

# enable MSVC_RUNTIME_LIBRARY target property
# see https://cmake.org/cmake/help/latest/policy/CMP0091.html
if(POLICY CMP0091)
  cmake_policy(SET CMP0091 NEW)
endif()

project(YAML_CPP VERSION 0.7.0 LANGUAGES CXX)

include(CMakePackageConfigHelpers)
include(CMakeDependentOption)
include(CheckCXXCompilerFlag)
include(GNUInstallDirs)
include(CTest)
include(GenerateExportHeader)

SET(CMAKE_CXX_STANDARD 17)
SET(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_C_VISIBILITY_PRESET hidden)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN 1)

option(YAML_BUILD_SHARED_LIBS "Build yaml-cpp shared library" ${BUILD_SHARED_LIBS})

set(yaml-cpp-type STATIC)
set(yaml-cpp-label-postfix "static")
if (YAML_BUILD_SHARED_LIBS)
  set(yaml-cpp-type SHARED)
  set(yaml-cpp-label-postfix "shared")
endif()

set(contrib-pattern "src/contrib/*.cpp")
set(src-pattern "src/*.cpp")

INCLUDE_DIRECTORIES(${PROJECT_SOURCE_DIR}/include)
file(GLOB_RECURSE yaml-cpp-contrib-sources ${contrib-pattern})
file(GLOB_RECURSE yaml-cpp-sources ${src-pattern})

SET(LIBRARY_OUTPUT_PATH ${CMAKE_SOURCE_DIR}/libs)

add_library(yaml-cpp ${yaml-cpp-type} ${yaml-cpp-contrib-sources} ${yaml-cpp-sources})
add_library(yaml-cpp::yaml-cpp ALIAS yaml-cpp)
