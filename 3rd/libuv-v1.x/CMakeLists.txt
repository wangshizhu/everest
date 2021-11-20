cmake_minimum_required(VERSION 3.4)
project(libuv LANGUAGES C)

set(CMAKE_C_VISIBILITY_PRESET hidden)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)
set(CMAKE_C_STANDARD 90)

set(uv_sources
    src/fs-poll.c
    src/idna.c
    src/inet.c
    src/random.c
    src/strscpy.c
    src/threadpool.c
    src/timer.c
    src/uv-common.c
    src/uv-data-getter-setters.c
    src/version.c)

list(APPEND uv_defines _FILE_OFFSET_BITS=64 _LARGEFILE_SOURCE _GNU_SOURCE _POSIX_C_SOURCE=200112)
list(APPEND uv_libraries pthread dl rt)

list(APPEND uv_sources
       src/unix/async.c
       src/unix/core.c
       src/unix/dl.c
       src/unix/fs.c
       src/unix/getaddrinfo.c
       src/unix/getnameinfo.c
       src/unix/loop-watcher.c
       src/unix/loop.c
       src/unix/pipe.c
       src/unix/poll.c
       src/unix/process.c
       src/unix/random-devurandom.c
       src/unix/signal.c
       src/unix/stream.c
       src/unix/tcp.c
       src/unix/thread.c
       src/unix/tty.c
       src/unix/udp.c
       src/unix/proctitle.c
       src/unix/linux-core.c
       src/unix/linux-inotify.c
       src/unix/linux-syscalls.c
       src/unix/procfs-exepath.c
       src/unix/random-getrandom.c
       src/unix/random-sysctl-linux.c
       src/unix/epoll.c)

SET(LIBRARY_OUTPUT_PATH ${CMAKE_SOURCE_DIR}/libs)
add_library(uv STATIC ${uv_sources})

target_compile_definitions(uv PRIVATE ${uv_defines})
target_compile_options(uv PRIVATE ${uv_cflags})
target_include_directories(uv
  PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
  PRIVATE
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/src>)

target_link_libraries(uv ${uv_libraries})