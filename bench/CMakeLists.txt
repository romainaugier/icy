# SPDX-License-Identifier: BSD-3-Clause
# Copyright (c) 2026 - Present Romain Augier
# All rights reserved.

set(TEST_LIBS icy::icy)

if(MINGW)
    list(APPEND TEST_LIBS -lstdc++exp)
endif()

file(GLOB_RECURSE bench_files *.cpp)

foreach(bench_src ${bench_files})
    get_filename_component(bench_name ${bench_src} NAME_WLE)
    message(STATUS "Added bench: ${bench_name}")

    add_executable(${bench_name} bench.cpp)

    target_include_directories(${bench_name} PRIVATE ${CMAKE_SOURCE_DIR}/ext/robin-map/include)

    target_link_libraries(${bench_name} PRIVATE ${TEST_LIBS})

    if(MSVC)
        target_compile_options(${bench_name} PRIVATE /O2 /DNDEBUG)
    else()
        target_compile_options(${bench_name} PRIVATE -O3 -DNDEBUG)
    endif()

    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        include(CheckCXXCompilerFlag)

        check_cxx_compiler_flag(-march=native ICY_HAS_MARCH_NATIVE)

        if(ICY_HAS_MARCH_NATIVE)
            target_compile_options(${bench_name} PRIVATE -march=native)
        endif()
    endif()
endforeach()