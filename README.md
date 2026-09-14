# icy

[![CI](https://github.com/romainaugier/icy/actions/workflows/ci.yml/badge.svg?branch=dev)](https://github.com/romainaugier/icy/actions/workflows/ci.yml?query=branch%3Adev)
[![Release](https://img.shields.io/github/v/release/romainaugier/icy?include_prereleases&sort=semver&label=release)](https://github.com/romainaugier/icy/releases)
[![License: BSD-3-Clause](https://img.shields.io/badge/License-BSD--3--Clause-blue.svg)](LICENSE)
[![C++23](https://img.shields.io/badge/C%2B%2B-23-00599C.svg)](https://en.cppreference.com/w/cpp/23)
[![Header-only](https://img.shields.io/badge/header--only-yes-success.svg)](include/icy.hpp)

Compile-time perfect hash map/set for C++23.

The tables are built entirely at compile-time via `consteval`, so runtime lookups are a single hash + open-addressed probe with zero initialization cost.

## Usage

```cpp
#include <icy.hpp>

constexpr auto colors = icy::map<std::string_view, int, 3>::make({
    {"red",   0xFF0000},
    {"green", 0x00FF00},
    {"blue",  0x0000FF},
});

if(auto it = colors.find("red"); it != colors.end())
    use(it->second);

for(const auto& [k, v] : colors) { /* iterate */ }

constexpr auto keywords = icy::set<std::string_view, 4>::make({
    "if", "else", "for", "while",
});

static_assert(keywords.contains("if"));
static_assert(keywords.find("class") == keywords.end());

constexpr auto primes = icy::set<int, 5>::make({2, 3, 5, 7, 11});
static_assert(*primes.find(7) == 7);
```

## Build & test (CMake)

```sh
cmake -S . -B build -DICY_BUILD_TESTS=ON
cmake --build build --config RelWithDebInfo
ctest --test-dir build --output-on-failure
```

## Build & benchmark (CMake)

```sh
cmake -S . -B build -DICY_BUILD_BENCH=ON
cmake --build build --config RelWithDebInfo
./build/bench/icy_bench
```

## Notes

- Best for small key sets.
- Duplicate keys are caught at compile-time and the `consteval` constructor throws and the compiler emits a diagnostic
- `table_size == 2 * N` (load factor 0.5), resolved via linear probing
- Keys must be `==`-comparable and hashable by `icy::detail::hash_key` (integral types and anything convertible to `std::string_view`.)