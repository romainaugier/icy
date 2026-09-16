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

BENCH_SRCS := $(wildcard bench/*.cpp)
BENCH_TARGETS := $(patsubst bench/%.cpp,build/bench/%,$(BENCH_SRCS))

bench: $(BENCH_TARGETS)

build/bench/%: bench/%.cpp build_dir
	cc ${INCLUDES} ${FLAGS} ${LINK} ${OPT_FLAGS} $< -o $@
	chmod +x $@

run_bench: bench
	echo "== bench =="
	@for t in $(BENCH_TARGETS); do echo "== $$t =="; $$t || exit 1; done

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