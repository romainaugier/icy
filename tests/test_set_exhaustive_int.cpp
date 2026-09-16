// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2026 - Present Romain Augier
// All rights reserved.

#define ICY_OPTIMIZE_KEY_CMP
#include <icy.hpp>

#include <array>
#include <cassert>
#include <cstdint>
#include <print>

#if !defined(ICY_EXHAUSTIVE_TEST_COUNT)
#define ICY_EXHAUSTIVE_TEST_COUNT (1ULL << 32)
#endif // defined(ICY_EXHAUSTIVE_TEST_COUNT)

constexpr std::uint64_t test_hash(std::uint64_t x) noexcept
{
    x += 0x9e3779b97f4a7c15ULL;
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
    x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;

    return x ^ (x >> 31);
}

// Compile-time fixtures

constexpr auto values = [] {
    std::array<std::uint64_t, 64> result{};

    for(std::size_t i = 0; i < result.size(); ++i)
        result[i] = test_hash(i);

    return result;
}();

constexpr auto values_set = icy::set<std::uint64_t, values.size()>::make_array(values);

// Compile-time correctness

static_assert(values_set.size() == values.size());
static_assert(!values_set.empty());

static_assert([] {
    for(auto value : values)
    {
        if(!values_set.contains(value))
            return false;
    }

    return true;
}());

// Runtime exhaustive test

int main(void)
{
    std::println("icy::set<int> exhaustive test begin");

#if defined(ICY_OPTIMIZE_KEY_CMP)
    std::println("key comparison: optimized");
#else
    std::println("key comparison: full");
#endif // defined(ICY_OPTIMIZE_KEY_CMP)

    std::uint64_t hits = 0;

    // Every inserted value must always be found.

    for(auto value : values)
    {
        assert(values_set.contains(value));
        ++hits;
    }

    // Exhaustive lookup over the low 32-bit domain.

    for(std::uint64_t value = 0; value < ICY_EXHAUSTIVE_TEST_COUNT; ++value)
    {
        const bool expected = [&] {
            for(auto item : values)
            {
                if(item == value)
                    return true;
            }

            return false;
        }();

        const bool actual = values_set.contains(value);

        assert(actual == expected);

        if(actual)
            ++hits;
    }

    // Deterministic pseudo-random 64-bit queries.
    //
    // These exercise the full hash input space rather than just
    // sequential integer keys.

    for(std::uint64_t i = 0; i < ICY_EXHAUSTIVE_TEST_COUNT; ++i)
    {
        const std::uint64_t value = test_hash(i + 0x123456789abcdef0ULL);

        const bool expected = [&] {
            for(auto item : values)
            {
                if(item == value)
                    return true;
            }

            return false;
        }();

        assert(values_set.contains(value) == expected);
    }

    // Force known hits into the pseudo-random test stream.

    for(std::uint64_t i = 0; i < values.size(); ++i)
    {
        const auto value =
            values[i];

        assert(values_set.contains(value));
    }

    std::println("icy::set<int> exhaustive test passed ({} iterations)",
                 ICY_EXHAUSTIVE_TEST_COUNT);

    return 0;
}