cmake_minimum_required(VERSION 3.25)

include(CheckFunctionExists)
include(CheckIncludeFile)

add_library(confuse OBJECT confuse.c lexer.c)

target_include_directories(confuse PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_compile_definitions(confuse PRIVATE PACKAGE_VERSION="3.4-dev")
target_compile_definitions(confuse PRIVATE PACKAGE_STRING="libConfuse 3.4-dev")
target_compile_definitions(confuse PUBLIC BUILDING_STATIC)

check_include_file("unistd.h" HAVE_UNISTD_H)

if (HAVE_UNISTD_H)
    target_compile_definitions(confuse PRIVATE HAVE_UNISTD_H)
endif()

check_include_file("string.h" HAVE_STRING_H)

if (HAVE_STRING_H)
    target_compile_definitions(confuse PRIVATE HAVE_STRING_H)
endif()

check_include_file("strings.h" HAVE_STRINGS_H)

if (HAVE_STRINGS_H)
    target_compile_definitions(confuse PRIVATE HAVE_STRINGS_H)
endif()

check_include_file("sys/stat.h" HAVE_SYS_STAT_H)

if (HAVE_SYS_STAT_H)
    target_compile_definitions(confuse PRIVATE HAVE_SYS_STAT_H)
endif()

check_include_file("windows.h" HAVE_WINDOWS_H)

if (HAVE_WINDOWS_H)
    target_compile_definitions(confuse PRIVATE HAVE_WINDOWS_H)
endif()

check_function_exists(fmemopen HAVE_FMEMOPEN)

if (HAVE_FMEMOPEN)
    target_compile_definitions(confuse PRIVATE HAVE_FMEMOPEN)
else()
    target_sources(confuse PRIVATE fmemopen.c)
endif()

check_function_exists(funopen HAVE_FUNOPEN)

if (HAVE_FUNOPEN)
    target_compile_definitions(confuse PRIVATE HAVE_FUNOPEN)
endif()

check_function_exists(reallocarray HAVE_REALLOCARRAY)

if (HAVE_REALLOCARRAY)
    target_compile_definitions(confuse PRIVATE HAVE_REALLOCARRAY)
else()
    target_sources(confuse PRIVATE reallocarray.c)
endif()

check_function_exists(strcasecmp HAVE_STRCASECMP)

if (HAVE_STRCASECMP)
    target_compile_definitions(confuse PRIVATE HAVE_STRCASECMP)
endif()

check_function_exists(strdup HAVE_STRDUP)

if (HAVE_STRDUP)
    target_compile_definitions(confuse PRIVATE HAVE_STRDUP)
endif()

check_function_exists(strndup HAVE_STRNDUP)

if (HAVE_STRNDUP)
    target_compile_definitions(confuse PRIVATE HAVE_STRNDUP)
endif()
