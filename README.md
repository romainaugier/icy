# icy

[![CI](https://github.com/romainaugier/icy/actions/workflows/ci.yml/badge.svg?branch=dev)](https://github.com/romainaugier/icy/actions/workflows/ci.yml?query=branch%3Adev)
[![Release](https://img.shields.io/github/v/release/romainaugier/icy?include_prereleases&sort=semver&label=release)](https://github.com/romainaugier/icy/releases)
[![License: BSD-3-Clause](https://img.shields.io/badge/License-BSD--3--Clause-blue.svg)](LICENSE)
[![C++23](https://img.shields.io/badge/C%2B%2B-23-00599C.svg)](https://en.cppreference.com/w/cpp/23)
[![Header-only](https://img.shields.io/badge/header--only-yes-success.svg)](include/icy.hpp)

Compile-time perfect hash map/set for C++23.

The tables are built entirely at compile-time via `consteval`, so runtime lookups are a single hash + open-addressed probe with zero initialization cost.

With all optimizations enabled, icy provides a 1.5~2x speedup compared to a static const std::unordered_map/set, without sacrificing accuracy and reliability.

## Usage

```cpp
#define ICY_OPTIMIZE_KEY_CMP // skip key comparison in find(), see notes below
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

## Benchmark

All numbers are **cycles/op**. Lower is better. **Bold** = best in row. `0.000` means below timer resolution.

Benchmarks were done on a **M4 Max** with **64 Go** of ram.

### `map find()`

| Key type | N | icy::map | std::map | std::unordered_map | tsl::robin_map | sorted vector+lb |
|---|---:|---:|---:|---:|---:|---:|
| `std::string_view` | 16 | **3.074** | 13.639 | 5.055 | 4.358 | 10.433 |
| `std::string_view` | 512 | **2.958** | 36.847 | 5.246 | 4.592 | 22.661 |
| `std::string_view` | 1024 | **3.228** | 44.393 | 5.278 | 4.595 | 23.987 |
| `std::int64_t` | 16 | **0.6412** | 1.945 | 0.802 | 1.114 | 2.396 |
| `std::int64_t` | 512 | **0.861** | 6.954 | 0.935 | 0.979 | 6.257 |
| `std::int64_t` | 1024 | **0.873** | 7.498 | 0.888 | 0.967 | 7.498 |

### `set find()`

| Key type | N | icy::set | std::set | std::unordered_set | tsl::robin_set | sorted vector+bs |
|---|---:|---:|---:|---:|---:|---:|
| `std::string_view` | 16 | **2.831** | 13.391 | 4.882 | 4.298 | 10.248 |
| `std::string_view` | 512 | **2.883** | 38.596 | 4.902 | 4.748 | 21.927 |
| `std::string_view` | 1024 | **2.934** | 43.786 | 4.971 | 4.282 | 23.696 |
| `std::int64_t` | 16 | **0.508** | 2.208 | 1.216 | 0.915 | 2.381 |
| `std::int64_t` | 512 | 0.777 | 8.540 | 1.408 | **0.673** | 6.234 |
| `std::int64_t` | 1024 | **0.752** | 8.522 | 1.022 | 0.803 | 7.622 |

### Iteration

| Key type | N | icy::map | std::map | tsl::robin_map | icy::set | std::set | tsl::robin_set |
|---|---:|---:|---:|---:|---:|---:|---:|
| `std::string_view` | 16 | **0.016** | 0.707 | 0.558 | **0.016** | 0.648 | 0.566 |
| `std::string_view` | 512 | 0.074 | 1.569 | 0.848 | **0.000** | 1.476 | 0.819 |
| `std::string_view` | 1024 | 0.073 | 1.586 | 0.918 | **0.000** | 1.471 | 0.894 |
| `std::int64_t` | 16 | 0.014 | 0.712 | 0.576 | **0.013** | 0.628 | 0.562 |
| `std::int64_t` | 512 | 0.121 | 1.569 | 0.742 | **0.000** | 1.453 | 0.739 |
| `std::int64_t` | 1024 | 0.126 | 1.646 | 0.823 | **0.000** | 1.475 | 0.893 |

## Build & test

```sh
cmake -S . -B build -DICY_BUILD_TESTS=ON
cmake --build build --config RelWithDebInfo
ctest --test-dir build --output-on-failure

# or

make run_tests
```

## Build & benchmark

```sh
cmake -S . -B build -DICY_BUILD_BENCH=ON
cmake --build build --config RelWithDebInfo
./build/bench/icy_bench

# or

make run_bench
```

## Notes

- Best for small key sets (for larger key sets (>=1000), use -fconstexpr-steps=\<N\>)
- Duplicate keys are caught at compile-time, the `consteval` constructor throws and the compiler emits a diagnostic
- `table_size == 2 * N` (load factor 0.5), resolved via linear probing
- define ICY_MAX_BUILDING_ROUNDS for a more thorough search (defaults to 128)
- define ICY_OPTIMIZE_KEY_CMP to skip key comparison when searching (only check hash), good speed-up and well-supported (no hash collision, see exhaustive tests for reference)
- Keys must be `==`-comparable and hashable by `icy::detail::hash_key_base` (integral types and anything that has `data()` and `size()`)