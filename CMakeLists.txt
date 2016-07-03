cmake_minimum_required(VERSION 3.1)

project(mvar C)

set(CMAKE_THREAD_PREFER_PTHREAD True)
find_package(Threads REQUIRED)

add_library(mvar mvar.c mvar.h mvar-internal.h)
target_include_directories(mvar PUBLIC .)
set_property(TARGET mvar PROPERTY C_STANDARD 11)

add_executable(mvar-test test.c)
target_link_libraries(mvar-test mvar Threads::Threads)
