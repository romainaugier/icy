# SPDX-License-Identifier: BSD-3-Clause
# Copyright (c) 2026 - Present Romain Augier
# All rights reserved.

INCLUDES=-Iinclude -Iext/robin-map/include
LINK=-lstdc++
FLAGS=-std=c++23 -fconstexpr-steps=4000000000
OPT_FLAGS=-O3 -march=native

# build
build_dir:
	mkdir -p build
	mkdir -p build/tests
	mkdir -p build/bench

# bench
 
bench: bench/bench.cpp build_dir
	cc bench/bench.cpp -o build/bench/icy_bench ${INCLUDES} ${FLAGS} ${LINK} ${OPT_FLAGS}
	chmod +x build/bench/icy_bench

run_bench: bench
	echo "== bench =="
	build/bench/icy_bench

# tests

TEST_SRCS := $(wildcard tests/*.cpp)
TEST_TARGETS := $(patsubst tests/%.cpp,build/tests/%,$(TEST_SRCS))

tests: $(TEST_TARGETS)

build/tests/%: tests/%.cpp build_dir
	cc ${INCLUDES} ${FLAGS} ${LINK} ${OPT_FLAGS} $< -o $@

run_tests: tests
	echo "== tests =="
	@for t in $(TEST_TARGETS); do echo "== $$t =="; $$t || exit 1; done

clean:
	rm -rf build

all: bench tests

.PHONY: all bench run_bench tests run_tests clean