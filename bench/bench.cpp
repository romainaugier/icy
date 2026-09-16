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
//
// Larger configurations (512 / 1024 entries) build a perfect hash over
// that many keys at compile time, which some compilers' default constexpr
// budgets are too small for. If you see a "constexpr evaluation exceeded
// step limit" style error, add:
//   GCC:   -fconstexpr-ops-limit=4000000000
//   Clang: -fconstexpr-steps=4000000000

#define ICY_OPTIMIZE_KEY_CMP
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
#include <utility>
#include <vector>

#include <tsl/robin_map.h>
#include <tsl/robin_set.h>

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

#define NUM_TRIALS 128

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

// Compile-time key generation
//
// icy::map / icy::set build their perfect hash via consteval, so the key
// set for any given configuration has to be a genuine compile-time
// constant - it can't be produced by a normal runtime function taking N
// as an argument. Everything below is parameterized purely by template
// arguments so it can be called directly where a compile-time key array
// is needed.

BENCH_NAMESPACE_BEGIN

// splitmix64: small, fast, well-distributed constexpr PRNG. Not
// cryptographic, just needs to scatter benchmark keys reasonably.
constexpr std::uint64_t splitmix64_next(std::uint64_t& state) noexcept
{
    std::uint64_t z = (state += 0x9E3779B97F4A7C15ULL);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}

// std::string_view doesn't own its characters, so the generated text has
// to live somewhere with static storage duration for the views to stay
// valid - this is that storage. MaxLen is a fixed per-key buffer size;
// actual generated lengths vary within [min_len, max_len] at generate()
// time, giving the "mixed length" keys.
template <std::size_t N, std::size_t MaxLen>
struct string_key_storage
{
    std::array<std::array<char, MaxLen>, N> chars{};
    std::array<std::uint32_t, N> lengths{};
};

template <std::size_t N, std::size_t MaxLen>
consteval string_key_storage<N, MaxLen> generate_string_keys(std::uint64_t seed,
                                                              std::uint32_t min_len,
                                                              std::uint32_t max_len) noexcept
{
    constexpr std::string_view alphabet = "abcdefghijklmnopqrstuvwxyz";

    string_key_storage<N, MaxLen> result{};
    std::uint64_t state = seed;

    for(std::size_t i = 0; i < N; ++i)
    {
        bool duplicate;

        do
        {
            const std::uint32_t len = min_len + static_cast<std::uint32_t>(splitmix64_next(state) % (max_len - min_len + 1));

            for(std::uint32_t c = 0; c < len; ++c)
                result.chars[i][c] = alphabet[splitmix64_next(state) % alphabet.size()];

            result.lengths[i] = len;

            // A genuine duplicate would make icy::map/set's construction
            // throw, so keys are regenerated in place until distinct from
            // everything placed so far - cheap, since the key space is
            // large relative to N here.
            duplicate = false;

            for(std::size_t j = 0; j < i && !duplicate; ++j)
            {
                if(result.lengths[j] != len)
                    continue;

                bool equal = true;

                for(std::uint32_t c = 0; c < len; ++c)
                {
                    if(result.chars[j][c] != result.chars[i][c])
                    {
                        equal = false;
                        break;
                    }
                }

                duplicate = equal;
            }
        } while(duplicate);
    }

    return result;
}

template <std::size_t N, std::size_t MaxLen>
constexpr std::array<std::string_view, N> as_views(const string_key_storage<N, MaxLen>& storage) noexcept
{
    std::array<std::string_view, N> views{};

    for(std::size_t i = 0; i < N; ++i)
        views[i] = std::string_view(storage.chars[i].data(), storage.lengths[i]);

    return views;
}

template <std::size_t N>
consteval std::array<std::int64_t, N> generate_int_keys(std::uint64_t seed) noexcept
{
    std::array<std::int64_t, N> result{};
    std::uint64_t state = seed;

    for(std::size_t i = 0; i < N; ++i)
    {
        std::int64_t candidate{};
        bool duplicate;

        do
        {
            candidate = static_cast<std::int64_t>(splitmix64_next(state));

            duplicate = false;

            for(std::size_t j = 0; j < i; ++j)
            {
                if(result[j] == candidate)
                {
                    duplicate = true;
                    break;
                }
            }
        } while(duplicate);

        result[i] = candidate;
    }

    return result;
}

template <typename Key, std::size_t N>
consteval std::array<std::pair<Key, int>, N> make_indexed_pairs(const std::array<Key, N>& keys) noexcept
{
    std::array<std::pair<Key, int>, N> pairs{};

    for(std::size_t i = 0; i < N; ++i)
        pairs[i] = { keys[i], static_cast<int>(i) };

    return pairs;
}

BENCH_NAMESPACE_END

// Harness

ANON_NAMESPACE_BEGIN

volatile int g_sink = 0;

template <typename Body>
std::uint64_t measure(Body&& body, int iters) 
{
    body();

    std::uint64_t best = UINT64_MAX;

    for(int trial = 0; trial < NUM_TRIALS; ++trial)
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

template <typename Key, typename Value>
struct PairKeyCompare
{
    bool operator()(const std::pair<Key, Value>& a, const Key& b) const
    {
        return a.first < b;
    }

    bool operator()(const Key& a, const std::pair<Key, Value>& b) const
    {
        return a < b.first;
    }
};

// Everything from here down is generic over Key (std::string_view or
// std::int64_t in practice) and N - it only ever touches icy_map/icy_set
// through their ordinary (already-built) interface, never through
// consteval, so unlike the generators above it doesn't need N or the key
// data as template/compile-time arguments.
template <typename Key, std::size_t N>
void run_benchmark(const char* key_description, const std::array<Key, N>& keys,
                   const icy::map<Key, int, N>& icy_map,
                   const icy::set<Key, N>& icy_set)
{
    std::println("################################################################################");
    std::println("  N = {} | key = {}", N, key_description);
    std::println("################################################################################");
    std::println("");

    std::mt19937 rng(0xC0FFEE ^ static_cast<unsigned>(N));
    std::uniform_int_distribution<std::size_t> dist(0, N - 1);

    constexpr std::size_t n_queries = 1000;

    std::vector<Key> queries;
    queries.reserve(n_queries);

    for(std::size_t i = 0; i < n_queries; ++i)
        queries.push_back(keys[dist(rng)]);

    std::map<Key, int> std_map;
    std::unordered_map<Key, int> std_umap;
    tsl::robin_map<Key, int> robin_map;
    std::set<Key> std_set;
    std::unordered_set<Key> std_uset;
    tsl::robin_set<Key> robin_set;
    std::vector<std::pair<Key, int>> sorted_vec;

    for(std::size_t i = 0; i < N; ++i)
    {
        const int v = static_cast<int>(i);
        std_map.emplace(keys[i], v);
        std_umap.emplace(keys[i], v);
        robin_map.emplace(keys[i], v);
        std_set.emplace(keys[i]);
        std_uset.emplace(keys[i]);
        robin_set.emplace(keys[i]);
        sorted_vec.emplace_back(keys[i], v);
    }

    std::sort(sorted_vec.begin(), sorted_vec.end());

    constexpr int iters = 1000;
    const auto ops_per_body = static_cast<double>(n_queries * iters);

    const auto body_map_icy = [&] {
        int acc = 0;

        for(auto q : queries)
            if(auto it = icy_map.find(q); it != icy_map.end())
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

    const auto body_map_robin = [&] {
        int acc = 0;

        for(auto q : queries)
            if(auto it = robin_map.find(q); it != robin_map.end())
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
        { "tsl::robin_map", measure(body_map_robin, iters) / ops_per_body },
        { "sorted vector+lb", measure(body_map_sorted, iters) / ops_per_body },
    };

    print_section("map find()", map_results);

    const auto body_set_icy = [&] {
        int acc = 0;

        for(auto q : queries)
            if(icy_set.find(q) != icy_set.end())
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

    const auto body_set_robin = [&] {
        int acc = 0;

        for(auto q : queries)
            if(robin_set.find(q) != robin_set.end())
                ++acc;

        g_sink = acc;
    };

    const auto body_set_sorted = [&] {
        int acc = 0;

        for(auto q : queries)
            if(std::binary_search(sorted_vec.begin(),
                                  sorted_vec.end(),
                                  q,
                                  PairKeyCompare<Key, int>{}))
                ++acc;

        g_sink = acc;
    };

    std::vector<Result> set_results = {
        { "icy::set", measure(body_set_icy, iters) / ops_per_body },
        { "std::set", measure(body_set_std, iters) / ops_per_body },
        { "std::unordered_set", measure(body_set_uset, iters) / ops_per_body },
        { "tsl::robin_set", measure(body_set_robin, iters) / ops_per_body },
        { "sorted vector+bs", measure(body_set_sorted, iters) / ops_per_body },
    };

    print_section("set find()", set_results);

    const auto body_iter_icy = [&] {
        int acc = 0;

        for(const auto& [_, v] : icy_map)
            acc += v;

        g_sink = acc;
    };

    const auto body_iter_std = [&] {
        int acc = 0;

        for(const auto& [_, v] : std_map)
            acc += v;

        g_sink = acc;
    };

    const auto body_iter_robin_map = [&] {
        int acc = 0;

        for(const auto& [_, v] : robin_map)
            acc += v;

        g_sink = acc;
    };

    const auto body_iter_icy_set = [&] {
        int acc = 0;

        for(const auto& v : icy_set)
            acc += 1;

        g_sink = acc;
    };

    const auto body_iter_std_set = [&] {
        int acc = 0;

        for(const auto& v : std_set)
            acc += 1;

        g_sink = acc;
    };

    const auto body_iter_robin_set = [&] {
        int acc = 0;

        for(const auto& v : robin_set)
            acc += 1;

        g_sink = acc;
    };

    const double iter_ops = static_cast<double>(N) * iters;

    std::vector<Result> iter_results = {
        { "icy::map", measure(body_iter_icy, iters) / iter_ops },
        { "std::map", measure(body_iter_std, iters) / iter_ops },
        { "tsl::robin_map", measure(body_iter_robin_map, iters) / iter_ops },
        { "icy::set", measure(body_iter_icy_set, iters) / iter_ops },
        { "std::set", measure(body_iter_std_set, iters) / iter_ops },
        { "tsl::robin_set", measure(body_iter_robin_set, iters) / iter_ops },
    };

    print_section("iteration", iter_results);
}

// Builds a mixed-length string key configuration (keys distinct, lengths
// uniformly random in [MinLen, MaxLen]) and runs the full benchmark
// suite against it. Everything the icy containers need is generated from
// template arguments alone, per the note at the top of this file.
template <std::size_t N, std::uint32_t MinLen, std::uint32_t MaxLen>
void run_string_config()
{
    static_assert(MinLen >= 1 && MinLen <= MaxLen, "invalid length range");

    constexpr std::uint64_t seed = 0x9E3779B97F4A7C15ULL
        ^ (static_cast<std::uint64_t>(N) * 0xBF58476D1CE4E5B9ULL)
        ^ (static_cast<std::uint64_t>(MinLen) << 32)
        ^ static_cast<std::uint64_t>(MaxLen);

    static constexpr auto storage = bench::generate_string_keys<N, MaxLen>(seed, MinLen, MaxLen);
    static constexpr auto keys = bench::as_views(storage);

    static constexpr auto icy_map_inst = icy::map<std::string_view, int, N>::make(bench::make_indexed_pairs(keys));
    static constexpr auto icy_set_inst = icy::set<std::string_view, N>::make_array(keys);

    std::size_t min_seen = keys[0].size(), max_seen = keys[0].size();
    double sum = 0.0;

    for(auto& k : keys)
    {
        min_seen = std::min(min_seen, k.size());
        max_seen = std::max(max_seen, k.size());
        sum += static_cast<double>(k.size());
    }

    char info[96];
    std::snprintf(info, sizeof(info), "std::string_view (mixed length, %zu-%zu chars, avg %.1f)",
                  min_seen, max_seen, sum / static_cast<double>(N));

    run_benchmark<std::string_view, N>(info, keys, icy_map_inst, icy_set_inst);
}

template <std::size_t N>
void run_int_config()
{
    constexpr std::uint64_t seed = 0x2545F4914F6CDD1DULL ^ (static_cast<std::uint64_t>(N) * 0x9E3779B97F4A7C15ULL);

    static constexpr auto keys = bench::generate_int_keys<N>(seed);

    static constexpr auto icy_map_inst = icy::map<std::int64_t, int, N>::make(bench::make_indexed_pairs(keys));
    static constexpr auto icy_set_inst = icy::set<std::int64_t, N>::make_array(keys);

    run_benchmark<std::int64_t, N>("std::int64_t (distinct random values)", keys, icy_map_inst, icy_set_inst);
}

ANON_NAMESPACE_END

// main

int main(void)
{
    std::println("icy benchmark (cycle source: {}, num trials: {})", bench::cycle_source(), NUM_TRIALS);
    std::println("");

    run_string_config<16, 4, 64>();
    run_string_config<512, 4, 64>();
    run_string_config<1024, 4, 64>();

    run_int_config<16>();
    run_int_config<512>();
    run_int_config<1024>();

    std::println("(sink = {})", static_cast<int>(g_sink));

    return 0;
}