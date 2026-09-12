// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2026 - Present Romain Augier
// All rights reserved.

// Cycle counter sources:
//   * x86_64 (MSVC)          : __rdtsc()          + compiler barriers
//   * x86_64 (GCC / Clang)   : lfence; rdtsc      (serializing)
//   * aarch64 (GCC / Clang)  : isb; mrs cntvct_el0  (Apple Silicon, Linux, clang-cl)
//   * aarch64 (MSVC)         : QueryPerformanceCounter fallback
//   * anything else          : std::chrono::steady_clock fallback
//
// Build with optimizations:
//   cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DICY_BUILD_BENCH=ON
//   cmake --build build --target icy_bench
//   ./build/bench/icy_bench

#include <icy.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <map>
#include <print>
#include <random>
#include <set>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#if defined(_MSC_VER)
#if !defined(WIN32_LEAN_AND_MEAN)
#define WIN32_LEAN_AND_MEAN
#endif // !defined(WIN32_LEAN_AND_MEAN)
#include <intrin.h>
#include <windows.h>
#endif // defined(_MSC_VER)

#if !defined(_MSC_VER)
#include <chrono>
#endif // !defined(_MSC_VER)

#define BENCH_NAMESPACE_BEGIN namespace bench {
#define BENCH_NAMESPACE_END }

#define ANON_NAMESPACE_BEGIN namespace {
#define ANON_NAMESPACE_END }

BENCH_NAMESPACE_BEGIN

// Cycle counter

[[nodiscard]] inline std::uint64_t read_cycles() noexcept 
{
#if defined(_MSC_VER) && (defined(_M_X64) || defined(_M_IX86))
    // Windows x86 / x64
    _ReadWriteBarrier();
    const std::uint64_t t = __rdtsc();
    _ReadWriteBarrier();

    return t;
#elif (defined(__x86_64__) || defined(__i386__)) && (defined(__GNUC__) || defined(__clang__))
    // GCC / Clang x86 / x64 (Linux, macOS, MinGW, WSL)
    std::uint32_t lo, hi;
    __asm__ __volatile__("lfence\n\trdtsc"
                         : "=a"(lo), "=d"(hi)
                         :
                         : "memory");

    return (static_cast<std::uint64_t>(hi) << 32) | lo;
#elif (defined(__aarch64__) || defined(_M_ARM64)) && (defined(__GNUC__) || defined(__clang__))
    // GCC / Clang ARM64 (Apple Silicon, Linux, clang-cl)
    // CNTVCT_EL0 is the userspace-accessible virtual counter 
    // ISB serializes the read (prevents OoO reordering past the barrier)
    std::uint64_t t;
    __asm__ __volatile__("isb\n\tmrs %0, cntvct_el0"
                         : "=r"(t)
                         :
                         : "memory");

    return t;
#elif defined(_MSC_VER) && defined(_M_ARM64)
    // MSVC ARM64
    // No inline asm and _ReadStatusReg's CNTVCT_EL0 encoding is fragile
    LARGE_INTEGER t;
    QueryPerformanceCounter(&t);

    return static_cast<std::uint64_t>(t.QuadPart);
#else
    return static_cast<std::uint64_t>(std::chrono::steady_clock::now().time_since_epoch().count());
#endif // defined(_MSC_VER) && (defined(_M_X64) || defined(_M_IX86))
}

[[nodiscard]] constexpr const char* cycle_source() noexcept 
{
#if defined(_MSC_VER) && (defined(_M_X64) || defined(_M_IX86))
    return "RDTSC (MSVC)";
#elif (defined(__x86_64__) || defined(__i386__)) && (defined(__GNUC__) || defined(__clang__))
    return "RDTSC + LFENCE";
#elif (defined(__aarch64__) || defined(_M_ARM64)) && (defined(__GNUC__) || defined(__clang__))
    return "CNTVCT_EL0 + ISB";
#elif defined(_MSC_VER) && defined(_M_ARM64)
    return "QueryPerformanceCounter (MSVC ARM64 fallback)";
#else
    return "std::chrono::steady_clock fallback (NOT cycles)";
#endif // defined(_MSC_VER) && (defined(_M_X64) || defined(_M_IX86))
}

BENCH_NAMESPACE_END

// Fixtures

constexpr std::array<std::string_view, 16> keys16 = {
    "alpha", "bravo", "charlie", "delta",
    "echo",  "foxtrot", "golf", "hotel",
    "india", "juliet", "kilo", "lima",
    "mike",  "november", "oscar", "papa",
};

constexpr auto icy_map16 = []() consteval {
    std::array<std::pair<std::string_view, int>, 16> e{};

    for(std::size_t i = 0; i < 16; ++i)
        e[i] = { keys16[i], static_cast<int>(i) };

    return icy::map<std::string_view, int, 16>::make(e);
}();

constexpr auto icy_set16 = []() consteval {
    std::array<std::string_view, 16> e{};

    return icy::set<std::string_view, 16>::make_array(keys16);
}();

// Harness

ANON_NAMESPACE_BEGIN

volatile int g_sink = 0;

template <typename Body>
std::uint64_t measure(Body&& body, int iters) 
{
    body();

    std::uint64_t best = UINT64_MAX;

    for(int trial = 0; trial < 7; ++trial)
    {
        const auto t0 = bench::read_cycles();

        for(int i = 0; i < iters; ++i) 
            body();

        const auto t1 = bench::read_cycles();

        if(t1 - t0 < best)
            best = t1 - t0;
    }

    return best;
}

struct Result 
{
    const char* name;
    double cycles_per_op;
};

void print_section(const char* title, const std::vector<Result>& rs) 
{
    double slowest = 0.0;

    for(const auto& r : rs)
        slowest = std::max(slowest, r.cycles_per_op);

    std::println("=== {} ===", title);

    for(const auto& r : rs)
    {
        std::println(" - {} : {:.3f} cycles/op  ({:.1f}% of slowest)",
                    r.name,
                    r.cycles_per_op,
                    100.0 * r.cycles_per_op / slowest);
    }

    std::println("");
}

struct Compare
{
    bool operator()(const std::pair<std::string_view, int>& a,
                    std::string_view b) const
    {
        return a.first < b;
    }

    bool operator()(std::string_view a,
                    const std::pair<std::string_view, int>& b) const
    {
        return a < b.first;
    }
};

ANON_NAMESPACE_END

// main

int main(void)
{
    std::println("icy benchmark (cycle source: {})", bench::cycle_source());
    std::println("");

    std::mt19937 rng(0xC0FFEE);
    std::uniform_int_distribution<int> dist(0, 15);

    constexpr std::size_t n_queries = 1000;

    std::vector<std::string_view> queries;
    queries.reserve(n_queries);

    for(std::size_t i = 0; i < n_queries; ++i)
        queries.push_back(keys16[dist(rng)]);

    std::map<std::string_view, int> std_map;
    std::unordered_map<std::string_view, int> std_umap;
    std::set<std::string_view> std_set;
    std::unordered_set<std::string_view> std_uset;
    std::vector<std::pair<std::string_view, int>> sorted_vec;

    for(std::size_t i = 0; i < 16; ++i)
    {
        const int v = static_cast<int>(i);
        std_map.emplace(keys16[i], v);
        std_umap.emplace(keys16[i], v);
        std_set.emplace(keys16[i]);
        std_uset.emplace(keys16[i]);
        sorted_vec.emplace_back(keys16[i], v);
    }

    std::sort(sorted_vec.begin(), sorted_vec.end());

    constexpr int iters = 1000;
    const auto ops_per_body = static_cast<double>(n_queries * iters);

    const auto body_map_icy = [&] {
        int acc = 0;

        for(auto q : queries)
            if(auto it = icy_map16.find(q); it != icy_map16.end())
                acc += it->second;

        g_sink = acc;
    };

    const auto body_map_std = [&] {
        int acc = 0;

        for(auto q : queries)
            if(auto it = std_map.find(q); it != std_map.end())
                acc += it->second;

        g_sink = acc;
    };

    const auto body_map_umap = [&] {
        int acc = 0;

        for(auto q : queries)
            if(auto it = std_umap.find(q); it != std_umap.end())
                acc += it->second;

        g_sink = acc;
    };

    const auto body_map_sorted = [&] {
        int acc = 0;

        for(auto q : queries) 
        {
            auto it = std::lower_bound(sorted_vec.begin(),
                                       sorted_vec.end(),
                                       q,
                                       [](const auto& a, const auto& b) {
                                            return a.first < b;
                                       }
            );

            if(it != sorted_vec.end() && it->first == q)
                acc += it->second;
        }

        g_sink = acc;
    };

    std::vector<Result> map_results = {
        { "icy::map", measure(body_map_icy, iters) / ops_per_body },
        { "std::map", measure(body_map_std, iters) / ops_per_body },
        { "std::unordered_map", measure(body_map_umap, iters) / ops_per_body },
        { "sorted vector+lb", measure(body_map_sorted, iters) / ops_per_body },
    };

    print_section("icy::map<std::string_view, int, 16> — find()", map_results);

    const auto body_set_icy = [&] {
        int acc = 0;

        for(auto q : queries)
            if(icy_set16.find(q) != icy_set16.end())
                ++acc;

        g_sink = acc;
    };

    const auto body_set_std = [&] {
        int acc = 0;

        for(auto q : queries)
            if(std_set.find(q) != std_set.end())
                ++acc;

        g_sink = acc;
    };

    const auto body_set_uset = [&] {
        int acc = 0;

        for(auto q : queries)
            if(std_uset.find(q) != std_uset.end())
                ++acc;

        g_sink = acc;
    };

    const auto body_set_sorted = [&] {
        int acc = 0;

        for(auto q : queries)
            if(std::binary_search(sorted_vec.begin(),
                                  sorted_vec.end(),
                                  q,
                                  Compare{}))
                ++acc;

        g_sink = acc;
    };

    std::vector<Result> set_results = {
        { "icy::set", measure(body_set_icy, iters) / ops_per_body },
        { "std::set", measure(body_set_std, iters) / ops_per_body },
        { "std::unordered_set", measure(body_set_uset, iters) / ops_per_body },
        { "sorted vector+bs", measure(body_set_sorted, iters) / ops_per_body },
    };

    print_section("icy::set<std::string_view, 16> — find()", set_results);

    const auto body_iter_icy = [&] {
        int acc = 0;

        for(const auto& [_, v] : icy_map16)
            acc += v;

        g_sink = acc;
    };

    const auto body_iter_std = [&] {
        int acc = 0;

        for(const auto& [_, v] : std_map)
            acc += v;

        g_sink = acc;
    };

    const auto body_iter_icy_set = [&] {
        int acc = 0;

        for(const auto& v : icy_set16)
            acc += 1;

        g_sink = acc;
    };

    const auto body_iter_std_set = [&] {
        int acc = 0;

        for(const auto& v : std_set)
            acc += 1;

        g_sink = acc;
    };

    const double iter_ops = 16.0 * iters;

    std::vector<Result> iter_results = {
        { "icy::map", measure(body_iter_icy, iters) / iter_ops },
        { "std::map", measure(body_iter_std, iters) / iter_ops },
        { "icy::set", measure(body_iter_icy_set, iters) / iter_ops },
        { "std::set", measure(body_iter_std_set, iters) / iter_ops },
    };

    print_section("Iteration (16 elements)", iter_results);

    std::println("(sink = {})", static_cast<int>(g_sink));

    return 0;
}