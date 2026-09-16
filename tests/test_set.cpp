// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2026 - Present Romain Augier
// All rights reserved.

#include <icy.hpp>

#include <algorithm>
#include <cassert>
#include <print>
#include <string_view>
#include <type_traits>
#include <vector>

// Compile-time fixtures

constexpr auto keywords = icy::set<std::string_view, 6>::make({
    "if", "else", "for", "while", "return", "break",
});

constexpr auto primes = icy::set<int, 5>::make({2, 3, 5, 7, 11});

// Size

static_assert(keywords.size() == 6);
static_assert(!keywords.empty());
static_assert(primes.size() == 5);

// find hits

static_assert(keywords.find("if") != keywords.end());
static_assert(keywords.find("else") != keywords.end());
static_assert(keywords.find("for") != keywords.end());
static_assert(keywords.find("while") != keywords.end());
static_assert(keywords.find("return") != keywords.end());
static_assert(keywords.find("break") != keywords.end());

static_assert(*keywords.find("for") == std::string_view{"for"});

static_assert(primes.find(2) != primes.end());
static_assert(primes.find(3) != primes.end());
static_assert(primes.find(5) != primes.end());
static_assert(primes.find(7) != primes.end());
static_assert(primes.find(11) != primes.end());

static_assert(*primes.find(2) == 2);
static_assert(*primes.find(3) == 3);
static_assert(*primes.find(5) == 5);
static_assert(*primes.find(7) == 7);
static_assert(*primes.find(11) == 11);

// find misses

static_assert(keywords.find("class") == keywords.end());
static_assert(keywords.find("IF") == keywords.end());
static_assert(keywords.find("") == keywords.end());
static_assert(primes.find(4) == primes.end());
static_assert(primes.find(13) == primes.end());

// contains

static_assert(keywords.contains("while"));
static_assert(!keywords.contains("goto"));
static_assert(primes.contains(2));
static_assert(primes.contains(11));
static_assert(!primes.contains(9));

// constexpr iteration count

static_assert([] {
    std::size_t n = 0;
    for (auto k : keywords) { (void)k; ++n; }
    return n;
}() == 6);

// iterator == is const_iterator (frozen)

static_assert(std::is_same_v<decltype(keywords)::iterator,
                             decltype(keywords)::const_iterator>);

// Runtime checks

int main(void) 
{
    std::println("icy::set tests begin");

    // Runtime find / contains

    assert(keywords.find("for")   != keywords.end());
    assert(keywords.find("goto")  == keywords.end());
    assert(primes.contains(5));
    assert(!primes.contains(6));

    // Iterate; collect and compare against expected set

    std::vector<std::string_view> kw;

    for(auto k : keywords)
        kw.push_back(k);

    std::sort(kw.begin(), kw.end());

    std::vector<std::string_view> expected{
        "break", "else", "for", "if", "return", "while"
    };

    std::sort(expected.begin(), expected.end());

    assert(kw == expected);

    // Walk with explicit const_iterator

    std::size_t n = 0;

    for(auto i = primes.cbegin(); i != primes.cend(); ++i)
        ++n;

    assert(n == primes.size());

    // What we get out is const

    static_assert(std::is_same_v<decltype(*keywords.begin()),
                                 const std::string_view&>);

    std::println("icy::set tests end");
}