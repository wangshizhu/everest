cmake_minimum_required(VERSION 3.18)

PROJECT(circular_queue_test)

SET(CMAKE_CXX_FLAGS "-g -pthread")
SET(CMAKE_CXX_STANDARD 20)
SET(CMAKE_CXX_STANDARD_REQUIRED True)

IF(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    SET(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
ENDIF()

SET(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)

INCLUDE_DIRECTORIES(${PROJECT_SOURCE_DIR}/../../everest)

if (MSVC)
    add_compile_options(/utf-8)
endif()

add_subdirectory(${PROJECT_SOURCE_DIR}/../../3rd/googletest ${PROJECT_BINARY_DIR}/googletest)

ADD_EXECUTABLE(circular_queue_test circular_queue_test.cpp)

# libeverest.a 是统一编译单元，包含 packet_serialize.cpp，链接时还会需要 protobuf 符号。
TARGET_LINK_LIBRARIES(circular_queue_test
    ${PROJECT_SOURCE_DIR}/../../libs/${CMAKE_BUILD_TYPE}/libeverest.a
    ${PROJECT_SOURCE_DIR}/../../libs/${CMAKE_BUILD_TYPE}/libprotobuf.a
    gtest_main)
