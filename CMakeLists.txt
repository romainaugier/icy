# SPDX-License-Identifier: BSD-3-Clause
# Copyright (c) 2026 - Present Romain Augier
# All rights reserved.
 
cmake_minimum_required(VERSION 3.21)

project(icy
        VERSION 0.1.0
        DESCRIPTION "Compile-time perfect hash map and set for C++23"
        LANGUAGES CXX)

add_library(icy INTERFACE)
add_library(icy::icy ALIAS icy)

target_include_directories(icy INTERFACE
                           $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
                           $<INSTALL_INTERFACE:include>)

target_compile_features(icy INTERFACE cxx_std_23)

if(PROJECT_IS_TOP_LEVEL)
    option(ICY_BUILD_BENCH "Build icy benchmarks" OFF)

    include(CTest)

    if(BUILD_TESTING)
        add_subdirectory(tests)
    endif()

    if(ICY_BUILD_BENCH)
        add_subdirectory(bench)
    endif()

endif()