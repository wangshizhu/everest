cmake_minimum_required(VERSION 3.18)
# ---------------------------------------------------------------------------------------
# project name
# ---------------------------------------------------------------------------------------
PROJECT(everest)

# ---------------------------------------------------------------------------------------
# project init
# ---------------------------------------------------------------------------------------
SET(CMAKE_CXX_FLAGS "-g -pthread")
SET(CMAKE_CXX_STANDARD 20)
SET(CMAKE_CXX_STANDARD_REQUIRED True)

# ---------------------------------------------------------------------------------------
# default build type
# ---------------------------------------------------------------------------------------
IF(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    SET(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
ENDIF()

# ---------------------------------------------------------------------------------------
# option
# ---------------------------------------------------------------------------------------
OPTION(BUILD_YAML "build yaml that is yaml-cpp of jbeder" OFF)
OPTION(BUILD_SPDLOG "build spdlog" OFF)
OPTION(BUILD_PROTOBUF "build protobuf" OFF)
OPTION(BUILD_REDISPLUS "build redis-plus-plus" OFF)
OPTION(BUILD_FMT "build fmt" ON)

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
# fmt
# ---------------------------------------------------------------------------------------
if (BUILD_FMT)
    add_subdirectory(${PROJECT_SOURCE_DIR}/3rd/fmt)
endif(BUILD_FMT)

# ---------------------------------------------------------------------------------------
# everest head directory
# ---------------------------------------------------------------------------------------
INCLUDE_DIRECTORIES(${PROJECT_SOURCE_DIR}/3rd/asio-1.16.1/include)
INCLUDE_DIRECTORIES(${PROJECT_SOURCE_DIR}/3rd/spd/include)
INCLUDE_DIRECTORIES(${PROJECT_SOURCE_DIR}/3rd/fmt/include)
INCLUDE_DIRECTORIES(${PROJECT_SOURCE_DIR}/3rd/random/include)
INCLUDE_DIRECTORIES(${PROJECT_SOURCE_DIR}/3rd/date/include)
INCLUDE_DIRECTORIES(${PROJECT_SOURCE_DIR}/everest)
# 无锁模式包含 3rd/lock-free-queue/... 中的头文件，需要 3rd 根目录。
INCLUDE_DIRECTORIES(${PROJECT_SOURCE_DIR}/3rd)
# packet_serialize.cpp 包含 protobuf 头文件，需要内置 protobuf 的 src 目录。
INCLUDE_DIRECTORIES(${PROJECT_SOURCE_DIR}/3rd/protobuf-3.18.0/src)

# ---------------------------------------------------------------------------------------
# everest source code
# ---------------------------------------------------------------------------------------
# 统一编译单元：只编译 include.cpp，它按依赖顺序 #include 其余所有 .cpp。
# 不能用 AUX_SOURCE_DIRECTORY 收集整个目录，否则 helper.cpp 会被单独编译，
# 而它有意不包含自己的头文件（依赖 include.cpp 先包含 common/include.h）。
SET(src_list ${PROJECT_SOURCE_DIR}/everest/common/src/include.cpp)

# ---------------------------------------------------------------------------------------
# fuzzy_search (header-only)
# ---------------------------------------------------------------------------------------
# fuzzy_search 是 header-only 库，不参与统一编译单元，没有可编译的 .cpp。
# 这里把头文件列入目标只为便于在 IDE 中浏览，实际编译覆盖由 test/fuzzy_search_test 承担；
# 使用方可以 target_link_libraries(... fuzzy_search) 引入其头文件目录。
SET(fuzzy_search_headers
    ${PROJECT_SOURCE_DIR}/everest/fuzzy_search/fuzzy_search_system.h
    ${PROJECT_SOURCE_DIR}/everest/fuzzy_search/fuzzy_search_types.h
    ${PROJECT_SOURCE_DIR}/everest/fuzzy_search/detail/fuzzy_search_base_index.h
    ${PROJECT_SOURCE_DIR}/everest/fuzzy_search/detail/fuzzy_search_delta_index.h
    ${PROJECT_SOURCE_DIR}/everest/fuzzy_search/detail/fuzzy_search_gram_key.h
    ${PROJECT_SOURCE_DIR}/everest/fuzzy_search/detail/fuzzy_search_lifecycle.h
    ${PROJECT_SOURCE_DIR}/everest/fuzzy_search/detail/fuzzy_search_posting.h
    ${PROJECT_SOURCE_DIR}/everest/fuzzy_search/detail/fuzzy_search_utf8.h
)

ADD_LIBRARY(fuzzy_search INTERFACE)
TARGET_INCLUDE_DIRECTORIES(fuzzy_search INTERFACE
    ${PROJECT_SOURCE_DIR}/everest
    ${PROJECT_SOURCE_DIR}/3rd/spd/include)

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
SET(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/libs/${CMAKE_BUILD_TYPE})

ADD_LIBRARY(${PROJECT_NAME} STATIC ${src_list} ${fuzzy_search_headers})

if (MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /utf-8)
endif()

# ---------------------------------------------------------------------------------------
# compile definitions
# ---------------------------------------------------------------------------------------
# if (BUILD_SPDLOG)
#     target_compile_definitions(${PROJECT_NAME} PUBLIC SPDLOG_COMPILED_LIB)
# endif()

SET_TARGET_PROPERTIES(${PROJECT_NAME} PROPERTIES OUTPUT_NAME ${PROJECT_NAME})