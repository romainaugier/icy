// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2026 - Present Romain Augier
// All rights reserved.

#define ICY_OPTIMIZE_KEY_CMP
#include <icy.hpp>

#include <array>
#include <cassert>
#include <charconv>
#include <cstdint>
#include <print>
#include <string_view>

#if !defined(ICY_EXHAUSTIVE_TEST_COUNT)
#define ICY_EXHAUSTIVE_TEST_COUNT (1ULL << 30)
#endif // !defined(ICY_EXHAUSTIVE_TEST_COUNT)

constexpr auto keywords = icy::set<std::string_view, 64>::make({
    "d",
    "a",
    "b",
    "c",
    "aa",
    "ab",
    "ba",
    "bb",
    "aaa",
    "aab",
    "aba",
    "abb",
    "baa",
    "bab",
    "bba",
    "bbb",
    "key",
    "key0",
    "key1",
    "key2",
    "key3",
    "key4",
    "key5",
    "key6",
    "key7",
    "key8",
    "key9",
    "KEY",
    "Key",
    "KEY0",
    "Key0",
    "hello",
    "world",
    "return",
    "continue",
    "break",
    "while",
    "for",
    "if",
    "else",
    "struct",
    "class",
    "constexpr",
    "consteval",
    "noexcept",
    "static",
    "inline",
    "template",
    "typename",
    "namespace",
    "operator",
    "nullptr",
    "uint64_t",
    "std::string_view",
    "0123456789",
    "abcdefghijklmnopqrstuvwxyz",
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ",
    "key_0",
    "key_1",
    "key_2",
    "key_3",
    "key_4",
    "key_5",
});

static_assert(keywords.size() == 64);

static_assert([] {
    for(auto key : keywords)
    {
        if(!keywords.contains(key))
            return false;

        if(keywords.find(key) == keywords.end())
            return false;
    }

    return true;
}());

static std::string_view make_number(std::uint64_t value, char* buffer) noexcept
{
    const auto result = std::to_chars(buffer,
                                      buffer + 32,
                                      value);

    assert(result.ec == std::errc{});

    return {
        buffer,
        static_cast<std::size_t>(result.ptr - buffer)
    };
}

int main(void)
{
    std::println("icy::set<string_view> exhaustive test begin");

#ifdef ICY_OPTIMIZE_KEY_CMP
    std::println("key comparison: optimized");
#else
    std::println("key comparison: full");
#endif

    // Every key in the table must be found.

    for(auto key : keywords)
    {
        assert(keywords.contains(key));
        assert(keywords.find(key) != keywords.end());
    }

    /*
     * Sequential numeric strings.
     *
     * None of these are in the table except for values that happen
     * to match one of the explicit string fixtures.
     *
     * This is particularly useful with ICY_OPTIMIZE_KEY_CMP:
     * a hash collision must never turn a missing key into a hit.
     */

    for(std::uint64_t value = 0; value < ICY_EXHAUSTIVE_TEST_COUNT; ++value)
    {
        char buffer[32];

        const auto key = make_number(value, buffer);

        const bool expected = keywords.contains(key);

        const bool actual = keywords.find(key) != keywords.end();

        assert(actual == expected);
    }

    /*
     * Deterministic generated strings.
     *
     * The changing prefix/suffix exercises different string lengths
     * and byte positions while keeping the test allocation-free.
     */

    for(std::uint64_t i = 0; i < ICY_EXHAUSTIVE_TEST_COUNT; ++i)
    {
        char buffer[64];

        const auto prefix = i & 1 ? std::string_view{"key_"} : std::string_view{"KEY_"};

        const auto number = make_number(i >> 1, buffer + prefix.size());

        char key_buffer[64];

        for(std::size_t j = 0; j < prefix.size(); ++j)
            key_buffer[j] = prefix[j];

        for(std::size_t j = 0; j < number.size(); ++j)
            key_buffer[prefix.size() + j] = number[j];

        const std::string_view key{
            key_buffer,
            prefix.size() + number.size()
        };

        const bool expected = keywords.contains(key);

        const bool actual = keywords.find(key) != keywords.end();

        assert(actual == expected);
    }

    /*
     * Single-byte mutations of every stored key.
     *
     * These are particularly valuable because they produce keys that
     * are extremely close to real keys while still being different.
     */

    for(auto original : keywords)
    {
        if(original.empty())
            continue;

        std::array<char, 128> buffer{};

        assert(original.size() <= buffer.size());

        for(std::size_t i = 0; i < original.size(); ++i)
            buffer[i] = original[i];

        for(std::size_t i = 0; i < original.size(); ++i)
        {
            const char original_byte = buffer[i];

            buffer[i] = original_byte == 'x' ? 'y' : 'x';

            const std::string_view mutated{
                buffer.data(),
                original.size()
            };

            assert(mutated != original);
            assert(!keywords.contains(mutated));

            buffer[i] = original_byte;
        }
    }

    std::println("icy::set<string_view> exhaustive test passed ({} iterations)",
                 ICY_EXHAUSTIVE_TEST_COUNT);

    return 0;
}