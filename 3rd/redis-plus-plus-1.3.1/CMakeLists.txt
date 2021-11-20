cmake_minimum_required(VERSION 3.1)

set(REDIS_PLUS_PLUS_VERSION "1.3.1")
message(STATUS "redis-plus-plus version: ${REDIS_PLUS_PLUS_VERSION}")

project(redis++ LANGUAGES CXX VERSION ${REDIS_PLUS_PLUS_VERSION})

set(REDIS_PLUS_PLUS_DEFAULT_BUILD_TYPE "Release")
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE ${REDIS_PLUS_PLUS_DEFAULT_BUILD_TYPE} CACHE STRING "Set build type" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "RelWithDebInfo" "MinSizeRel")
endif()
message(STATUS "redis-plus-plus build type: ${CMAKE_BUILD_TYPE}")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++1z")

# libuv
set(REDIS_PLUS_PLUS_ASYNC_LIB_HEADER ${CMAKE_SOURCE_DIR}/3rd/libuv-v1.x/include)
set(REDIS_PLUS_PLUS_SOURCE_DIR src/sw/redis++)

set(REDIS_PLUS_PLUS_SOURCES
        "${REDIS_PLUS_PLUS_SOURCE_DIR}/command.cpp"
        "${REDIS_PLUS_PLUS_SOURCE_DIR}/command_options.cpp"
        "${REDIS_PLUS_PLUS_SOURCE_DIR}/connection.cpp"
        "${REDIS_PLUS_PLUS_SOURCE_DIR}/connection_pool.cpp"
        "${REDIS_PLUS_PLUS_SOURCE_DIR}/crc16.cpp"
        "${REDIS_PLUS_PLUS_SOURCE_DIR}/errors.cpp"
        "${REDIS_PLUS_PLUS_SOURCE_DIR}/pipeline.cpp"
        "${REDIS_PLUS_PLUS_SOURCE_DIR}/redis.cpp"
        "${REDIS_PLUS_PLUS_SOURCE_DIR}/redis_cluster.cpp"
        "${REDIS_PLUS_PLUS_SOURCE_DIR}/reply.cpp"
        "${REDIS_PLUS_PLUS_SOURCE_DIR}/sentinel.cpp"
        "${REDIS_PLUS_PLUS_SOURCE_DIR}/shards.cpp"
        "${REDIS_PLUS_PLUS_SOURCE_DIR}/shards_pool.cpp"
        "${REDIS_PLUS_PLUS_SOURCE_DIR}/subscriber.cpp"
        "${REDIS_PLUS_PLUS_SOURCE_DIR}/transaction.cpp"
        "${REDIS_PLUS_PLUS_SOURCE_DIR}/async_connection.cpp"
        "${REDIS_PLUS_PLUS_SOURCE_DIR}/async_connection_pool.cpp"
        "${REDIS_PLUS_PLUS_SOURCE_DIR}/async_redis.cpp"
        "${REDIS_PLUS_PLUS_SOURCE_DIR}/event_loop.cpp"
        "${REDIS_PLUS_PLUS_SOURCE_DIR}/async_sentinel.cpp"
        "${REDIS_PLUS_PLUS_SOURCE_DIR}/async_redis_cluster.cpp"
        "${REDIS_PLUS_PLUS_SOURCE_DIR}/async_shards_pool.cpp"
)

set(REDIS_PLUS_PLUS_ASYNC_FUTURE_HEADER "${REDIS_PLUS_PLUS_SOURCE_DIR}/future/std")

# cxx utils
set(CXX_UTILS_DIR "${REDIS_PLUS_PLUS_SOURCE_DIR}/cxx17")

# TLS support
option(REDIS_PLUS_PLUS_USE_TLS "Build with TLS support" OFF)
message(STATUS "redis-plus-plus TLS support: ${REDIS_PLUS_PLUS_USE_TLS}")

if(REDIS_PLUS_PLUS_USE_TLS)
    set(TLS_SUB_DIR "${REDIS_PLUS_PLUS_SOURCE_DIR}/tls")

    list(APPEND REDIS_PLUS_PLUS_SOURCES "${TLS_SUB_DIR}/tls.cpp")

    set(REDIS_PLUS_PLUS_DEPENDS "hiredis,hiredis_ssl")
else()
    set(TLS_SUB_DIR "${REDIS_PLUS_PLUS_SOURCE_DIR}/no_tls")

    set(REDIS_PLUS_PLUS_DEPENDS "hiredis")
endif()

# hiredis dependency
set(REDIS_PLUS_PLUS_HIREDIS_LIBS ${CMAKE_SOURCE_DIR}/libs/libhiredis.a)
set(REDIS_PLUS_PLUS_HIREDIS_HEADS ${CMAKE_SOURCE_DIR}/3rd)

# Build static library
option(REDIS_PLUS_PLUS_BUILD_STATIC "Build static library" ON)
message(STATUS "redis-plus-plus build static library: ${REDIS_PLUS_PLUS_BUILD_STATIC}")

if(REDIS_PLUS_PLUS_BUILD_STATIC)
    set(STATIC_LIB redis++_static)
    SET(LIBRARY_OUTPUT_PATH ${CMAKE_SOURCE_DIR}/libs)

    add_library(${STATIC_LIB} STATIC ${REDIS_PLUS_PLUS_SOURCES})

    target_include_directories(${STATIC_LIB} PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/${REDIS_PLUS_PLUS_SOURCE_DIR}>
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/${TLS_SUB_DIR}>
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/${CXX_UTILS_DIR}>
            $<BUILD_INTERFACE:${REDIS_PLUS_PLUS_HIREDIS_HEADS}>
            $<INSTALL_INTERFACE:include>)

    target_link_libraries(${STATIC_LIB} PUBLIC ${REDIS_PLUS_PLUS_HIREDIS_LIBS})

    target_include_directories(${STATIC_LIB} PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/${REDIS_PLUS_PLUS_ASYNC_FUTURE_HEADER}>)
    target_include_directories(${STATIC_LIB} PUBLIC $<BUILD_INTERFACE:${REDIS_PLUS_PLUS_ASYNC_LIB_HEADER}>)

    target_compile_options(${STATIC_LIB} PRIVATE "-Wall" "-W" "-Werror")

    set_target_properties(${STATIC_LIB} PROPERTIES OUTPUT_NAME redis++)
    set_target_properties(${STATIC_LIB} PROPERTIES CLEAN_DIRECT_OUTPUT 1)
    set_target_properties(${STATIC_LIB} PROPERTIES CXX_EXTENSIONS OFF)

    option(REDIS_PLUS_PLUS_BUILD_STATIC_WITH_PIC "Build static library with position independent code" ON)
    message(STATUS "redis-plus-plus build static library with position independent code: ${REDIS_PLUS_PLUS_BUILD_STATIC_WITH_PIC}")

    if(REDIS_PLUS_PLUS_BUILD_STATIC_WITH_PIC)
        set_target_properties(${STATIC_LIB} PROPERTIES POSITION_INDEPENDENT_CODE ON)
    endif()
endif()