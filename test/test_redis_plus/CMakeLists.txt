cmake_minimum_required(VERSION 3.18)

PROJECT(test_redis_plus)

SET(CMAKE_CXX_FLAGS "-g -pthread")
SET(CMAKE_CXX_STANDARD 17)
SET(CMAKE_CXX_STANDARD_REQUIRED True)
ADD_DEFINITIONS ( -std=c++1z)

MESSAGE(STATUS "src directory:"${PROJECT_BINARY_DIR})

INCLUDE_DIRECTORIES(${PROJECT_SOURCE_DIR}/../../3rd)
INCLUDE_DIRECTORIES(${PROJECT_SOURCE_DIR}/../../3rd/redis-plus-plus-1.3.1/src/sw/redis++)

SET(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)
ADD_EXECUTABLE(${PROJECT_NAME} main.cpp)
TARGET_LINK_LIBRARIES(${PROJECT_NAME} ${PROJECT_SOURCE_DIR}/../../libs/libhiredis.a)
TARGET_LINK_LIBRARIES(${PROJECT_NAME} ${PROJECT_SOURCE_DIR}/../../libs/libuv.a)
TARGET_LINK_LIBRARIES(${PROJECT_NAME} ${PROJECT_SOURCE_DIR}/../../libs/libredis++.a)

