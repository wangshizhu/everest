cmake_minimum_required(VERSION 3.18)

PROJECT(calculator_test)

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

ADD_EXECUTABLE(calculator_test safe_integral_calculator_test.cpp)

TARGET_LINK_LIBRARIES(calculator_test gtest_main)
