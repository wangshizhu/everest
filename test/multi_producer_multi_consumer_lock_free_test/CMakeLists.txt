cmake_minimum_required(VERSION 3.18)

PROJECT(multi_producer_multi_consumer_lock_free_test)

SET(CMAKE_CXX_FLAGS "-g -pthread")
SET(CMAKE_CXX_STANDARD 20)
SET(CMAKE_CXX_STANDARD_REQUIRED True)

IF(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    SET(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
ENDIF()

SET(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)

INCLUDE_DIRECTORIES(${PROJECT_SOURCE_DIR}/../../everest)
# 无锁模式包含 3rd/lock-free-queue/... 中的头文件，需要 3rd 根目录。
INCLUDE_DIRECTORIES(${PROJECT_SOURCE_DIR}/../../3rd)
# 共享用例头位于默认模式测试目录。
INCLUDE_DIRECTORIES(${PROJECT_SOURCE_DIR}/../multi_producer_multi_consumer_test)

if (MSVC)
    add_compile_options(/utf-8)
endif()

add_subdirectory(${PROJECT_SOURCE_DIR}/../../3rd/googletest ${PROJECT_BINARY_DIR}/googletest)

ADD_EXECUTABLE(multi_producer_multi_consumer_lock_free_test multi_producer_multi_consumer_lock_free_test.cpp)
TARGET_COMPILE_DEFINITIONS(multi_producer_multi_consumer_lock_free_test PRIVATE USE_LOCK_FREE_QUEUE)

# libeverest.a 是统一编译单元，包含 packet_serialize.cpp，链接时还会需要 protobuf 符号。
TARGET_LINK_LIBRARIES(multi_producer_multi_consumer_lock_free_test
    ${PROJECT_SOURCE_DIR}/../../libs/${CMAKE_BUILD_TYPE}/libeverest.a
    ${PROJECT_SOURCE_DIR}/../../libs/${CMAKE_BUILD_TYPE}/libprotobuf.a
    gtest_main)
