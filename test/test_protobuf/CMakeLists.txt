cmake_minimum_required(VERSION 3.18)

PROJECT(test_protobuf)

SET(CMAKE_CXX_FLAGS "-g -pthread")
SET(CMAKE_CXX_STANDARD 20)
SET(CMAKE_CXX_STANDARD_REQUIRED True)

INCLUDE_DIRECTORIES(${PROJECT_SOURCE_DIR}/../../3rd/protobuf-3.18.0/src)

file(GLOB_RECURSE headers "${PROJECT_SOURCE_DIR}/message/*.h")
file(GLOB_RECURSE sources "${PROJECT_SOURCE_DIR}/message/*.cc")

message(STATUS "headers:"${headers})
message(STATUS "sources:"${sources})

SET(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)
ADD_EXECUTABLE(${PROJECT_NAME} main.cpp ${headers} ${sources})

if (MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /utf-8)
endif()
TARGET_LINK_LIBRARIES(${PROJECT_NAME} ${PROJECT_SOURCE_DIR}/../../libs/libprotobuf.a)

