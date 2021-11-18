CMAKE_MINIMUM_REQUIRED(VERSION 3.4.0)
INCLUDE(GNUInstallDirs)
PROJECT(hiredis)

MACRO(getVersionBit name)
  SET(VERSION_REGEX "^#define ${name} (.+)$")
  FILE(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/hiredis.h"
    VERSION_BIT REGEX ${VERSION_REGEX})
  STRING(REGEX REPLACE ${VERSION_REGEX} "\\1" ${name} "${VERSION_BIT}")
ENDMACRO(getVersionBit)

getVersionBit(HIREDIS_MAJOR)
getVersionBit(HIREDIS_MINOR)
getVersionBit(HIREDIS_PATCH)
getVersionBit(HIREDIS_SONAME)
SET(VERSION "${HIREDIS_MAJOR}.${HIREDIS_MINOR}.${HIREDIS_PATCH}")
MESSAGE("Detected version: ${VERSION}")

PROJECT(hiredis VERSION "${VERSION}")

INCLUDE_DIRECTORIES(${PROJECT_SOURCE_DIR}/../)

SET(hiredis_sources
    alloc.c
    async.c
    dict.c
    hiredis.c
    net.c
    read.c
    sds.c
    sockcompat.c)

SET(hiredis_sources ${hiredis_sources})

file(GLOB_RECURSE hiredis_headers "${PROJECT_SOURCE_DIR}/*.h")

SET(LIBRARY_OUTPUT_PATH ${CMAKE_SOURCE_DIR}/libs)
ADD_LIBRARY(hiredis STATIC ${hiredis_headers} ${hiredis_sources})

