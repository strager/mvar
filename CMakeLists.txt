cmake_minimum_required(VERSION 3.1)

project(mvar C)

set(CMAKE_THREAD_PREFER_PTHREAD True)
find_package(Threads REQUIRED)

include(CheckIncludeFiles)
check_include_files(linux/futex.h HAVE_LINUX_FUTEX_H)
check_include_files(mach/semaphore.h HAVE_MACH_SEMAPHORE_H)

set(MVAR_USE_GENERATION_FUTEX False)
set(MVAR_USE_MACH_SEMAPHORE False)
if (HAVE_LINUX_FUTEX_H)
	set(MVAR_USE_GENERATION_FUTEX True)
elseif (HAVE_MACH_SEMAPHORE_H)
	set(MVAR_USE_MACH_SEMAPHORE True)
else ()
	message(SEND_ERROR "Unsupported platform")
endif ()
configure_file(mvar-internal-config.h.in mvar-internal-config.h)

add_library(mvar mvar.c mvar.h mvar-internal.h)
target_include_directories(mvar PUBLIC .)
target_include_directories(mvar PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
set_property(TARGET mvar PROPERTY C_STANDARD 11)

add_executable(mvar-test test.c)
target_link_libraries(mvar-test mvar Threads::Threads)
