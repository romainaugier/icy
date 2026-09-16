# SPDX-License-Identifier: BSD-3-Clause
# Copyright (c) 2026 - Present Romain Augier
# All rights reserved.

set(TEST_LIBS icy::icy)

if(MINGW)
    list(APPEND TEST_LIBS -lstdc++exp)
endif()

file(GLOB_RECURSE test_files *.cpp)

foreach(test_src ${test_files})
    get_filename_component(test_name ${test_src} NAME_WLE)
    message(STATUS "Added test: ${test_name}")

    add_executable(${test_name} ${test_src})
    target_link_libraries(${test_name} PRIVATE ${TEST_LIBS})

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

    add_test(NAME ${test_name} COMMAND $<TARGET_FILE:${test_name}>)
endforeach()