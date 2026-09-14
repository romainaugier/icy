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

    add_test(NAME ${test_name} COMMAND $<TARGET_FILE:${test_name}>)
endforeach()