# SPDX-License-Identifier: BSD-3-Clause
# Copyright (c) 2026 - Present Romain Augier
# All rights reserved.

set(LIBS icy::icy)

if(MINGW)
    list(APPEND LIBS -lstdc++exp)
endif()

add_executable(profiling profiling.cpp)
target_link_libraries(profiling PRIVATE ${LIBS})

if(MSVC)
    target_compile_options(profiling PRIVATE /O2 /DNDEBUG)
else()
    target_compile_options(profiling PRIVATE -O3 -DNDEBUG -g3)
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    include(CheckCXXCompilerFlag)

    check_cxx_compiler_flag(-march=native ICY_HAS_MARCH_NATIVE)

    if(ICY_HAS_MARCH_NATIVE)
        target_compile_options(profiling PRIVATE -march=native)
    endif()
endif()