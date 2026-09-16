# SPDX-License-Identifier: BSD-3-Clause
# Copyright (c) 2026 - Present Romain Augier
# All rights reserved.

set(TEST_LIBS icy::icy)

if(MINGW)
    list(APPEND TEST_LIBS -lstdc++exp)
endif()

add_executable(icy_bench bench.cpp)

target_include_directories(icy_bench PRIVATE ${CMAKE_SOURCE_DIR}/ext/robin-map/include)

target_link_libraries(icy_bench PRIVATE ${TEST_LIBS})

if(MSVC)
    target_compile_options(icy_bench PRIVATE /O2 /DNDEBUG)
else()
    target_compile_options(icy_bench PRIVATE -O3 -DNDEBUG)
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    include(CheckCXXCompilerFlag)

    check_cxx_compiler_flag(-march=native ICY_HAS_MARCH_NATIVE)

    if(ICY_HAS_MARCH_NATIVE)
        target_compile_options(icy_bench PRIVATE -march=native)
    endif()
endif()