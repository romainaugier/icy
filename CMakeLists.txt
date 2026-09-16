# SPDX-License-Identifier: BSD-3-Clause
# Copyright (c) 2026 - Present Romain Augier
# All rights reserved.
 
cmake_minimum_required(VERSION 3.21)

project(icy VERSION 0.1.0)

add_library(icy INTERFACE)
add_library(icy::icy ALIAS icy)

target_include_directories(icy INTERFACE
                           $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
                           $<INSTALL_INTERFACE:include>)

target_compile_features(icy INTERFACE cxx_std_23)

if(PROJECT_IS_TOP_LEVEL)
    option(ICY_BUILD_BENCH "Build icy benchmarks" OFF)
    option(ICY_BUILD_TESTS "Build icy tests" OFF)

    include(CTest)

    if(ICY_BUILD_TESTS)
        if(ICY_EXHAUSTIVE_TEST_COUNT)
            message(STATUS "Exhaustive test count set: ${ICY_EXHAUSTIVE_TEST_COUNT}")
            add_definitions(-DICY_EXHAUSTIVE_TEST_COUNT=${ICY_EXHAUSTIVE_TEST_COUNT})
        endif()

        enable_testing()
        add_subdirectory(tests)
    endif()

    if(ICY_BUILD_BENCH)
        add_subdirectory(bench)
    endif()

endif()