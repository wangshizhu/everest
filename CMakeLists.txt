cmake_minimum_required(VERSION 3.18)
# ---------------------------------------------------------------------------------------
# project name
# ---------------------------------------------------------------------------------------
PROJECT(everest)

# ---------------------------------------------------------------------------------------
# project init
# ---------------------------------------------------------------------------------------
SET(CMAKE_CXX_FLAGS "-g -pthread")
SET(CMAKE_CXX_STANDARD 17)
SET(CMAKE_CXX_STANDARD_REQUIRED True)
ADD_DEFINITIONS ( -std=c++1z)

# ---------------------------------------------------------------------------------------
# option
# ---------------------------------------------------------------------------------------
OPTION(BUILD_YAML "build yaml that is yaml-cpp of jbeder" OFF)
OPTION(BUILD_SPDLOG "build spdlog" OFF)
OPTION(BUILD_PROTOBUF "build protobuf" OFF)
OPTION(BUILD_REDISPLUS "build redis-plus-plus" ON)

# ---------------------------------------------------------------------------------------
# yaml
# ---------------------------------------------------------------------------------------
if (BUILD_YAML)
    add_subdirectory(${PROJECT_SOURCE_DIR}/3rd/yaml)
endif(BUILD_YAML)

# ---------------------------------------------------------------------------------------
# spdlog
# ---------------------------------------------------------------------------------------
set(SPDLOG_ALL_SRCS)
if (BUILD_SPDLOG)
    INCLUDE_DIRECTORIES(${PROJECT_SOURCE_DIR}/3rd)
    add_subdirectory(${PROJECT_SOURCE_DIR}/3rd/spd)
endif(BUILD_SPDLOG)

# ---------------------------------------------------------------------------------------
# protobuf
# ---------------------------------------------------------------------------------------
if (BUILD_PROTOBUF)
    add_subdirectory(${PROJECT_SOURCE_DIR}/3rd/protobuf-3.18.0/cmake)
endif(BUILD_PROTOBUF)

# ---------------------------------------------------------------------------------------
# redis-plus-plus
# ---------------------------------------------------------------------------------------
if (BUILD_REDISPLUS)
    add_subdirectory(${PROJECT_SOURCE_DIR}/3rd/hiredis)
    add_subdirectory(${PROJECT_SOURCE_DIR}/3rd/libuv-v1.x)
    add_subdirectory(${PROJECT_SOURCE_DIR}/3rd/redis-plus-plus-1.3.1)
endif(BUILD_REDISPLUS)

# ---------------------------------------------------------------------------------------
# everest source code
# ---------------------------------------------------------------------------------------
AUX_SOURCE_DIRECTORY(${PROJECT_SOURCE_DIR}/cmd_line src_list)
AUX_SOURCE_DIRECTORY(${PROJECT_SOURCE_DIR}/format src_list)

MESSAGE(STATUS "Build type: " ${CMAKE_BUILD_TYPE})
MESSAGE(STATUS "src list:"${src_list})
MESSAGE(STATUS "project name:"${PROJECT_NAME})
MESSAGE(STATUS "cmake_binary_dir:"${CMAKE_BINARY_DIR})
MESSAGE(STATUS "cmake_source_dir:"${CMAKE_SOURCE_DIR})
MESSAGE(STATUS "project_binary_dir:"${PROJECT_BINARY_DIR})
MESSAGE(STATUS "project_source_dir:"${PROJECT_SOURCE_DIR})
MESSAGE(STATUS "cmake_current_binary_dir:"${CMAKE_CURRENT_BINARY_DIR})
MESSAGE(STATUS "cmake_current_source_dir:"${CMAKE_CURRENT_SOURCE_DIR})

# ---------------------------------------------------------------------------------------
# change library directory
# ---------------------------------------------------------------------------------------
SET(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/libs)

ADD_LIBRARY(${PROJECT_NAME} STATIC ${src_list})

# ---------------------------------------------------------------------------------------
# compile definitions
# ---------------------------------------------------------------------------------------
# if (BUILD_SPDLOG)
#     target_compile_definitions(${PROJECT_NAME} PUBLIC SPDLOG_COMPILED_LIB)
# endif()

SET_TARGET_PROPERTIES(${PROJECT_NAME} PROPERTIES OUTPUT_NAME ${PROJECT_NAME})