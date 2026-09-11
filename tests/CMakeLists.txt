# SPDX-License-Identifier: BSD-3-Clause
# Copyright (c) 2026 - Present Romain Augier
# All rights reserved.
 
add_executable(icy_test_map test_map.cpp)
target_link_libraries(icy_test_map PRIVATE icy::icy)
add_test(NAME icy.map COMMAND icy_test_map)

add_executable(icy_test_set test_set.cpp)
target_link_libraries(icy_test_set PRIVATE icy::icy)
add_test(NAME icy.set COMMAND icy_test_set)