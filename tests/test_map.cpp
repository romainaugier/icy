// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2026 - Present Romain Augier
// All rights reserved.

#include <icy.hpp>

#include <algorithm>
#include <cassert>
#include <print>
#include <string_view>
#include <vector>

// Compile-time fixtures

constexpr auto color_map = icy::map<std::string_view, int, 5>::make({
    {"red",   0xFF0000},
    {"green", 0x00FF00},
    {"blue",  0x0000FF},
    {"black", 0x000000},
    {"white", 0xFFFFFF},
});

constexpr auto num_map = icy::map<int, std::string_view, 3>::make({
    {1, "one"},
    {2, "two"},
    {3, "three"},
});

// Size / capacity

static_assert(color_map.size() == 5);
static_assert(color_map.capacity() == 10);
static_assert(!color_map.empty());
static_assert(num_map.size() == 3);

// Find hits (compile time)

static_assert(color_map.find("red") != color_map.end());
static_assert(color_map.find("green") != color_map.end());
static_assert(color_map.find("blue") != color_map.end());
static_assert(color_map.find("black") != color_map.end());
static_assert(color_map.find("white") != color_map.end());

static_assert(color_map.find("red")->second == 0xFF0000);
static_assert(color_map.find("green")->second == 0x00FF00);
static_assert(color_map.find("white")->second == 0xFFFFFF);

static_assert(num_map.find(1)->second == "one");
static_assert(num_map.find(3)->second == "three");

// Find misses (compile time)

static_assert(color_map.find("cyan") == color_map.end());
static_assert(color_map.find("RED") == color_map.end()); 
static_assert(color_map.find("") == color_map.end());
static_assert(num_map.find(4) == num_map.end());
static_assert(num_map.find(0) == num_map.end());

// Contains

static_assert( color_map.contains("blue"));
static_assert(!color_map.contains("yellow"));
static_assert( num_map.contains(2));
static_assert(!num_map.contains(99));

// constexpr iteration count

static_assert([] {
    std::size_t n = 0;
    for (const auto& [k, v] : color_map) { (void)k; (void)v; ++n; }
    return n;
}() == 5);

// iterator == is const_iterator (frozen)

static_assert(std::is_same_v<decltype(color_map)::iterator,
                             decltype(color_map)::const_iterator>);

// Runtime checks

int main(void)
{
    std::println("icy::map tests begin");

    // Runtime find

    auto it = color_map.find("black");
    assert(it != color_map.end());
    assert(it->second == 0x000000);

    assert(color_map.find("cyan") == color_map.end());

    // Iterate; collect keys and values

    std::vector<std::string_view> keys;

    int xor_all = 0;

    for(const auto& [k, v] : color_map)
    {
        keys.push_back(k);
        xor_all ^= v;
    }

    std::sort(keys.begin(), keys.end());

    const std::vector<std::string_view> expected{"black", "blue", "green", "red", "white"};

    assert(keys == expected);
    assert(xor_all == (0xFF0000 ^ 0x00FF00 ^ 0x0000FF ^ 0x000000 ^ 0xFFFFFF));

    // Explicit iterator walk matches range-for

    std::size_t manual_count = 0;

    for(auto i = color_map.cbegin(); i != color_map.cend(); ++i)
        ++manual_count;

    assert(manual_count == color_map.size());

    // Const-ness: what we get out is const

    static_assert(std::is_same_v<decltype(*color_map.begin()),
                                 const std::pair<const std::string_view, int>&>);

    std::println("icy::map tests end");
}