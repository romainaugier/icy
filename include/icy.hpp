// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2026 - Present Romain Augier
// All rights reserved.

#pragma once

#if !defined(__ICY)
#define __ICY

#define ICY_NAMESPACE_BEGIN namespace icy {
#define ICY_NAMESPACE_END }

#define DETAIL_NAMESPACE_BEGIN namespace detail {
#define DETAIL_NAMESPACE_END }

#if !defined(ICY_MAX_BUILDING_ROUNDS)
#define ICY_MAX_BUILDING_ROUNDS 128
#endif // !defined(ICY_MAX_BUILDING_ROUNDS)

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iterator>
#include <limits>
#include <string_view>
#include <type_traits>
#include <utility>

ICY_NAMESPACE_BEGIN

DETAIL_NAMESPACE_BEGIN

// cityhash

/*
 * Copyright (c) 2011 Google, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * CityHash, by Geoff Pike and Jyrki Alakuijala.
 *
 * This is a constexpr adaptation of CityHash64 and CityHash64WithSeed.
 */

constexpr std::uint64_t cityhash_k0 = 0xc3a5c85c97cb3127ULL;
constexpr std::uint64_t cityhash_k1 = 0xb492b66fbe98f273ULL;
constexpr std::uint64_t cityhash_k2 = 0x9ae16a3b2f90404fULL;

constexpr std::uint64_t rotate(std::uint64_t value, unsigned shift) noexcept
{
    return shift == 0 ? value : (value >> shift) | (value << (64 - shift));
}

constexpr std::uint64_t shift_mix(std::uint64_t value) noexcept
{
    return value ^ (value >> 47);
}

constexpr std::uint64_t fetch32(const char* data) noexcept
{
    if consteval 
    {
        return (static_cast<std::uint64_t>(static_cast<unsigned char>(data[0])))       |
               (static_cast<std::uint64_t>(static_cast<unsigned char>(data[1])) << 8)  |
               (static_cast<std::uint64_t>(static_cast<unsigned char>(data[2])) << 16) |
               (static_cast<std::uint64_t>(static_cast<unsigned char>(data[3])) << 24);
    }

    std::uint32_t value;
    std::memcpy(std::addressof(value), data, sizeof(std::uint32_t));
    return value;
}

constexpr std::uint64_t fetch64(const char* data) noexcept
{
    if consteval 
    {
        return (static_cast<std::uint64_t>(static_cast<unsigned char>(data[0])))       |
               (static_cast<std::uint64_t>(static_cast<unsigned char>(data[1])) << 8)  |
               (static_cast<std::uint64_t>(static_cast<unsigned char>(data[2])) << 16) |
               (static_cast<std::uint64_t>(static_cast<unsigned char>(data[3])) << 24) |
               (static_cast<std::uint64_t>(static_cast<unsigned char>(data[4])) << 32) |
               (static_cast<std::uint64_t>(static_cast<unsigned char>(data[5])) << 40) |
               (static_cast<std::uint64_t>(static_cast<unsigned char>(data[6])) << 48) |
               (static_cast<std::uint64_t>(static_cast<unsigned char>(data[7])) << 56);
    }

    std::uint64_t value;
    std::memcpy(std::addressof(value), data, sizeof(std::uint64_t));
    return value;
}

constexpr std::uint64_t hash_len_16(std::uint64_t u, std::uint64_t v) noexcept
{
    constexpr std::uint64_t mul = 0x9ddfea08eb382d69ULL;

    std::uint64_t a = (u ^ v) * mul;
    a ^= a >> 47;

    std::uint64_t b = (v ^ a) * mul;
    b ^= b >> 47;
    b *= mul;

    return b;
}

constexpr std::uint64_t hash_len_16(std::uint64_t u, std::uint64_t v, std::uint64_t mul) noexcept
{
    std::uint64_t a = (u ^ v) * mul;
    a ^= a >> 47;

    std::uint64_t b = (v ^ a) * mul;
    b ^= b >> 47;
    b *= mul;

    return b;
}

constexpr std::uint64_t hash_len_0_to_16(const char* data, std::size_t length) noexcept
{
    if(length >= 8)
    {
        const std::uint64_t mul = cityhash_k2 + length * 2;
        const std::uint64_t a = fetch64(data) + cityhash_k2;
        const std::uint64_t b = fetch64(data + length - 8);
        const std::uint64_t c = rotate(b, 37) * mul + a;
        const std::uint64_t d = (rotate(a, 25) + b) * mul;

        return hash_len_16(c, d, mul);
    }

    if(length >= 4)
    {
        const std::uint64_t mul = cityhash_k2 + length * 2;
        const std::uint64_t a = fetch32(data);

        return hash_len_16(length + (a << 3),
                           fetch32(data + length - 4),
                           mul);
    }

    if(length != 0)
    {
        const std::uint64_t a = static_cast<unsigned char>(data[0]);
        const std::uint64_t b = static_cast<unsigned char>(data[length >> 1]);
        const std::uint64_t c = static_cast<unsigned char>(data[length - 1]);
        const std::uint64_t y = a + (b << 8);
        const std::uint64_t z = length + (c << 2);

        return shift_mix(y * cityhash_k2 ^ z * cityhash_k0) * cityhash_k2;
    }

    return cityhash_k2;
}

constexpr std::uint64_t hash_len_17_to_32(const char* data, std::size_t length) noexcept
{
    const std::uint64_t mul = cityhash_k2 + length * 2;
    const std::uint64_t a = fetch64(data) * cityhash_k1;
    const std::uint64_t b = fetch64(data + 8);
    const std::uint64_t c = fetch64(data + length - 8) * mul;
    const std::uint64_t d = fetch64(data + length - 16) * cityhash_k2;

    return hash_len_16(rotate(a + b, 43) + rotate(c, 30) +
                       d,
                       a + rotate(b + cityhash_k2, 18) + c,
                       mul);
}

struct weak_hash
{
    std::uint64_t first;
    std::uint64_t second;
};

constexpr weak_hash weak_hash_len_32_with_seeds(std::uint64_t w,
                                                std::uint64_t x,
                                                std::uint64_t y,
                                                std::uint64_t z,
                                                std::uint64_t a,
                                                std::uint64_t b) noexcept
{
    a += w;
    b = rotate(b + a + z, 21);

    const std::uint64_t c = a;

    a += x;
    a += y;

    b += rotate(a, 44);

    return { a + z, b + c };
}

constexpr weak_hash weak_hash_len_32_with_seeds(const char* data,
                                                std::uint64_t a,
                                                std::uint64_t b) noexcept
{
    return weak_hash_len_32_with_seeds(fetch64(data),
                                       fetch64(data + 8),
                                       fetch64(data + 16),
                                       fetch64(data + 24),
                                       a,
                                       b);
}

constexpr std::uint64_t hash_len_33_to_64(const char* data, std::size_t length) noexcept
{
    const std::uint64_t mul = cityhash_k2 + length * 2;
    const std::uint64_t a = fetch64(data) * cityhash_k2;
    const std::uint64_t b = fetch64(data + 8);
    const std::uint64_t c = fetch64(data + length - 24);
    const std::uint64_t d = fetch64(data + length - 32);
    const std::uint64_t e = fetch64(data + 16) * cityhash_k2;
    const std::uint64_t f = fetch64(data + 24) * 9;
    const std::uint64_t g = fetch64(data + length - 8);
    const std::uint64_t h = fetch64(data + length - 16) * mul;
    const std::uint64_t u = rotate(a + g, 43) + (rotate(b, 30) + c) * 9;
    const std::uint64_t v = ((a + g) ^ d) + f + 1;
    const std::uint64_t w = std::byteswap((u + v) * mul) + h;
    const std::uint64_t x = rotate(e + f, 42) + c;
    const std::uint64_t y = (std::byteswap((v + w) * mul) + g) * mul;
    const std::uint64_t z = e + f + c;
    const std::uint64_t new_a = std::byteswap((x + z) * mul + y) + b;
    const std::uint64_t new_b = shift_mix((z + new_a) * mul + d + h) * mul;

    return new_b + x;
}

constexpr std::uint64_t city_hash_64(const char* data, std::size_t length) noexcept
{
    if(length <= 16)
        return hash_len_0_to_16(data, length);

    if(length <= 32)
        return hash_len_17_to_32(data, length);

    if(length <= 64)
        return hash_len_33_to_64(data, length);

    std::uint64_t x = fetch64(data + length - 40);
    std::uint64_t y = fetch64(data + length - 16) + fetch64(data + length - 56);
    std::uint64_t z = hash_len_16(fetch64(data + length - 48) + length,fetch64(data + length - 24));

    weak_hash v = weak_hash_len_32_with_seeds(data + length - 64,
                                              length,
                                              z);

    weak_hash w = weak_hash_len_32_with_seeds(data + length - 32,
                                              y + cityhash_k1,
                                              x);

    x = x * cityhash_k1 + fetch64(data);

    length = (length - 1) & ~static_cast<std::size_t>(63);

    do {
        x = rotate(x + y + v.first + fetch64(data + 8), 37) * cityhash_k1;
        y = rotate(y + v.second + fetch64(data + 48), 42) * cityhash_k1;
        x ^= w.second;
        y += v.first + fetch64(data + 40);
        z = rotate(z + w.first, 33) * cityhash_k1;

        v = weak_hash_len_32_with_seeds(data,
                                        v.second * cityhash_k1,
                                        x + w.first);

        w = weak_hash_len_32_with_seeds(data + 32,
                                        z + w.second,
                                        y + fetch64(data + 16));

        const std::uint64_t temporary = z;
        z = x;
        x = temporary;

        data += 64;
        length -= 64;
    } while (length != 0);

    return hash_len_16(hash_len_16(v.first, w.first) + shift_mix(y) * cityhash_k1 + z,
                       hash_len_16(v.second, w.second) + x);
}

constexpr std::uint64_t city_hash_64(std::string_view data) noexcept
{
    return city_hash_64(data.data(), data.size());
}

constexpr std::uint64_t city_hash_64_with_seed(const char* data,
                                               std::size_t length,
                                               std::uint64_t seed) noexcept
{
    return detail::hash_len_16(detail::city_hash_64(data, length) - detail::cityhash_k2,
                               seed);
}

constexpr std::uint64_t city_hash_64_with_seed(std::string_view data,
                                               std::uint64_t seed) noexcept
{
    return city_hash_64_with_seed(data.data(), data.size(), seed);
}

constexpr std::uint64_t mix64(std::uint64_t x) noexcept
{
    x ^= x >> 33;
    x *= 0xff51afd7ed558ccdull;
    x ^= x >> 33;
    x *= 0xc4ceb9fe1a85ec53ull;
    x ^= x >> 33;

    return x;
}

template <typename Key>
constexpr std::uint64_t hash_key(const Key& key, std::uint64_t seed) noexcept
{
    if constexpr (std::is_integral_v<Key>)
    {
        return mix64(static_cast<std::uint64_t>(key)) ^ seed;
    }
    else
    {
        return city_hash_64_with_seed(key.data(), key.size(), seed);
    }
}

constexpr std::size_t next_pow2(std::size_t n) noexcept
{
    if(n <= 1)
        return 1;

    --n;

    n |= n >> 1;
    n |= n >> 2;
    n |= n >> 4;
    n |= n >> 8;
    n |= n >> 16;
    n |= n >> 32;

    return n + 1;
}

constexpr std::size_t table_size_for(std::size_t n) noexcept
{
    if(n <= 1)
        return 1;

    return next_pow2(n * 4);
}

DETAIL_NAMESPACE_END

// icy::map

template<typename Key,
         typename Value,
         std::size_t N>
class map
{
    static_assert(N >= 1, "icy::map requires at least one entry");
    static_assert(N <= std::numeric_limits<std::uint32_t>::max(),
                  "icy::map supports at most UINT32_MAX entries");
    static_assert(ICY_MAX_BUILDING_ROUNDS >= 1,
                  "ICY_MAX_BUILDING_ROUNDS must be greater than zero");

public:
    using key_type = Key;
    using mapped_type = Value;
    using value_type = std::pair<const Key, Value>;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    using reference = const value_type&;
    using pointer = const value_type*;

private:
    static constexpr std::size_t TABLE_SIZE = detail::table_size_for(N);
    static constexpr std::size_t MASK = TABLE_SIZE - 1;
    static constexpr std::uint32_t EMPTY = std::numeric_limits<std::uint32_t>::max();

    using index_type = std::size_t;

    struct slot
    {
        index_type index{EMPTY};
        std::uint64_t hash{0};
    };

    static_assert(sizeof(slot) == 16, "sizeof(slot) must be 16 bytes");

    std::array<value_type, N> _values;
    std::array<slot, TABLE_SIZE> _table{};
    std::uint64_t _seed{0};

    struct build_result
    {
        std::array<slot, TABLE_SIZE> table{};

        std::uint64_t seed{0};

        std::size_t total_probes{0};
        std::size_t max_probe{0};

        bool valid{false};
    };

    template <typename Container>
    static consteval std::array<value_type, N> make_values(const Container& items)
    {
        std::array<value_type, N> values{};

        for(std::size_t i = 0; i < N; ++i)
            std::construct_at(std::addressof(values[i]), items[i].first, items[i].second);

        return values;
    }

    template <typename Container>
    static consteval build_result build(const Container& items)
    {
        build_result best{};

        for(std::size_t round = 0;
            round < ICY_MAX_BUILDING_ROUNDS;
            ++round)
        {
            build_result candidate{};

            candidate.seed = static_cast<std::uint64_t>(round);

            bool collision = false;
            bool duplicate = false;

            for(std::size_t value_index = 0;
                value_index < N;
                ++value_index)
            {
                const auto& key = items[value_index].first;

                const std::uint64_t hash = detail::hash_key(key, candidate.seed);

                std::size_t idx = hash & MASK;
                std::size_t probes = 0;

                while(candidate.table[idx].index != EMPTY)
                {
                    if(candidate.table[idx].hash == hash)
                    {
                        const std::size_t existing_index = candidate.table[idx].index;

                        if(items[existing_index].first == key)
                            duplicate = true;

                        collision = true;

                        break;
                    }

                    idx = (idx + 1) & MASK;
                    ++probes;
                }

                if(collision)
                    break;

                candidate.table[idx].index = static_cast<index_type>(value_index);
                candidate.table[idx].hash = hash;

                candidate.total_probes += probes;

                if(probes > candidate.max_probe)
                    candidate.max_probe = probes;
            }

            if(duplicate)
                throw "icy::map: duplicate key in initializer";

            candidate.valid = true;

            if(!best.valid ||
               candidate.max_probe < best.max_probe ||
               (candidate.max_probe == best.max_probe &&
                candidate.total_probes < best.total_probes))
            {
                best = candidate;
            }
        }

        return best;
    }

    template <typename Container>
    consteval map(const Container& items) : _values(make_values(items))
    {
        static_assert(sizeof(items) / sizeof(items[0]) == N,
                      "icy::map: initializer size mismatch");

        const build_result result = build(items);

        this->_table = result.table;
        this->_seed = result.seed;
    }

    [[nodiscard]] constexpr std::size_t find_index(const Key& key) const noexcept
    {
        const std::uint64_t hash = detail::hash_key(key, this->_seed);

        std::size_t idx = hash & MASK;

        while(this->_table[idx].index != EMPTY)
        {
            const slot& current = this->_table[idx];

            if(current.hash == hash)
            {
                const std::size_t value_index = current.index;

#if defined(ICY_OPTIMIZE_KEY_CMP)
                return value_index;
#else
                if(this->_values[value_index].first == key)
                    return value_index;
#endif // defined(ICY_OPTIMIZE_KEY_CMP)
            }

            idx = (idx + 1) & MASK;
        }

        return N;
    }

public:
    static consteval map make(const std::pair<Key, Value>(&items)[N])
    {
        return map(items);
    }

    static consteval map make(const std::array<std::pair<Key, Value>, N>& items)
    {
        return map(items);
    }

    class const_iterator
    {
    private:
        friend class map;

        const value_type* _values_ptr{nullptr};
        std::size_t _ptr{0};

        constexpr const_iterator(const value_type* values,
                                 std::size_t index) noexcept :
            _values_ptr(values),
            _ptr(index)
        {
        }

    public:
        using iterator_category = std::forward_iterator_tag;
        using iterator_concept = std::forward_iterator_tag;
        using value_type = map::value_type;
        using difference_type = std::ptrdiff_t;
        using pointer = const value_type*;
        using reference = const value_type&;

        const_iterator() = default;

        constexpr reference operator*() const noexcept
        {
            return this->_values_ptr[this->_ptr];
        }

        constexpr pointer operator->() const noexcept
        {
            return &this->_values_ptr[this->_ptr];
        }

        constexpr const_iterator& operator++() noexcept
        {
            ++this->_ptr;
            return *this;
        }

        constexpr const_iterator operator++(int) noexcept
        {
            auto tmp = *this;
            ++(*this);
            return tmp;
        }

        friend constexpr bool operator==(const const_iterator& a,
                                         const const_iterator& b) noexcept
        {
            return a._values_ptr == b._values_ptr &&
                   a._ptr == b._ptr;
        }

        friend constexpr bool operator!=(const const_iterator& a,
                                         const const_iterator& b) noexcept
        {
            return !(a == b);
        }
    };

    using iterator = const_iterator;

    [[nodiscard]] constexpr const_iterator begin() const noexcept
    {
        return const_iterator(this->_values.data(), 0);
    }

    [[nodiscard]] constexpr const_iterator end() const noexcept
    {
        return const_iterator(this->_values.data(), N);
    }

    [[nodiscard]] constexpr const_iterator cbegin() const noexcept
    {
        return this->begin();
    }

    [[nodiscard]] constexpr const_iterator cend() const noexcept
    {
        return this->end();
    }

    [[nodiscard]] constexpr const_iterator find(const Key& key) const noexcept
    {
        const std::size_t value_index = this->find_index(key);

        if(value_index == N)
            return this->end();

        return const_iterator(this->_values.data(), value_index);
    }

    [[nodiscard]] constexpr bool contains(const Key& key) const noexcept
    {
        return this->find_index(key) != N;
    }

    [[nodiscard]] constexpr std::size_t size() const noexcept
    {
        return N;
    }

    [[nodiscard]] constexpr std::size_t capacity() const noexcept
    {
        return TABLE_SIZE;
    }

    [[nodiscard]] constexpr bool empty() const noexcept
    {
        return false;
    }
};

// icy::set

template<typename Key,
         std::size_t N>
class set
{
    static_assert(N >= 1, "icy::set requires at least one element");
    static_assert(N <= std::numeric_limits<std::uint32_t>::max(),
                  "icy::set supports at most UINT32_MAX entries");
    static_assert(ICY_MAX_BUILDING_ROUNDS >= 1,
                  "ICY_MAX_BUILDING_ROUNDS must be greater than zero");

public:
    using key_type = Key;
    using value_type = Key;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    using reference = const value_type&;
    using pointer = const value_type*;

private:
    static constexpr std::size_t TABLE_SIZE = detail::table_size_for(N);
    static constexpr std::size_t MASK = TABLE_SIZE - 1;
    static constexpr std::uint32_t EMPTY = std::numeric_limits<std::uint32_t>::max();

    using index_type = std::size_t;

    struct slot
    {
        index_type index{EMPTY};
        std::uint64_t hash{0};
    };

    static_assert(sizeof(slot) == 16, "sizeof(slot) must be 16 bytes");

    std::array<value_type, N> _values;
    std::array<slot, TABLE_SIZE> _table{};
    std::uint64_t _seed{0};

    struct build_result
    {
        std::array<slot, TABLE_SIZE> table{};

        std::uint64_t seed{0};

        std::size_t total_probes{0};
        std::size_t max_probe{0};

        bool valid{false};
    };

    template <typename Container>
    static consteval std::array<value_type, N> make_values(const Container& items)
    {
        std::array<value_type, N> values{};

        for(std::size_t i = 0; i < N; ++i)
            values[i] = items[i];

        return values;
    }

    template <typename Container>
    static consteval build_result build(const Container& items)
    {
        build_result best{};

        for(std::size_t round = 0; round < ICY_MAX_BUILDING_ROUNDS; ++round)
        {
            build_result candidate{};

            candidate.seed = static_cast<std::uint64_t>(round);

            bool collision = false;
            bool duplicate = false;

            for(std::size_t value_index = 0; value_index < N; ++value_index)
            {
                const auto& key = items[value_index];

                const std::uint64_t hash = detail::hash_key(key, candidate.seed);

                std::size_t idx = hash & MASK;
                std::size_t probes = 0;

                while(candidate.table[idx].index != EMPTY)
                {
                    if(candidate.table[idx].hash == hash)
                    {
                        const std::size_t existing_index = candidate.table[idx].index;

                        if(items[existing_index] == key)
                            duplicate = true;

                        collision = true;

                        break;
                    }

                    idx = (idx + 1) & MASK;
                    ++probes;
                }

                if(collision)
                    break;

                candidate.table[idx].index = static_cast<index_type>(value_index);
                candidate.table[idx].hash = hash;

                candidate.total_probes += probes;

                if(probes > candidate.max_probe)
                    candidate.max_probe = probes;
            }

            if(duplicate)
                throw "icy::set: duplicate key in initializer";

            if(collision)
                continue;

            candidate.valid = true;

            if(!best.valid ||
               candidate.max_probe < best.max_probe ||
               (candidate.max_probe == best.max_probe &&
                candidate.total_probes < best.total_probes))
            {
                best = candidate;
            }
        }

        return best;
    }

    template <typename Container>
    consteval set(const Container& items) : _values(make_values(items))
    {
        static_assert(sizeof(items) / sizeof(items[0]) == N,
                      "icy::set: initializer size mismatch");

        const build_result result = build(items);

        if(!result.valid)
            throw "icy::set: could not find a collision-free table within ICY_MAX_BUILDING_ROUNDS rounds";

        this->_table = result.table;
        this->_seed = result.seed;
    }

    [[nodiscard]] constexpr std::size_t find_index(const Key& key) const noexcept
    {
        const std::uint64_t hash = detail::hash_key(key, this->_seed);

        std::size_t idx = hash & MASK;

        while(this->_table[idx].index != EMPTY)
        {
            const slot& current = this->_table[idx];

            if(current.hash == hash)
            {
                const std::size_t value_index = current.index;

#if defined(ICY_OPTIMIZE_KEY_CMP)
                return value_index;
#else
                if(this->_values[value_index] == key)
                    return value_index;
#endif // defined(ICY_OPTIMIZE_KEY_CMP)
            }

            idx = (idx + 1) & MASK;
        }

        return N;
    }

public:
    static consteval set make(const Key(&items)[N])
    {
        return set(items);
    }

    static consteval set make_array(const std::array<Key, N>& items)
    {
        return set(items);
    }

    class const_iterator
    {
    private:
        friend class set;

        const value_type* _values_ptr{nullptr};

        std::size_t _ptr{0};

        constexpr const_iterator(const value_type* values,
                                 std::size_t index) noexcept : _values_ptr(values),
                                                               _ptr(index)
        {}

    public:
        using iterator_category = std::forward_iterator_tag;
        using iterator_concept = std::forward_iterator_tag;
        using value_type = set::value_type;
        using difference_type = std::ptrdiff_t;
        using pointer = const value_type*;
        using reference = const value_type&;

        const_iterator() = default;

        constexpr reference operator*() const noexcept
        {
            return this->_values_ptr[this->_ptr];
        }

        constexpr pointer operator->() const noexcept
        {
            return &this->_values_ptr[this->_ptr];
        }

        constexpr const_iterator& operator++() noexcept
        {
            ++this->_ptr;
            return *this;
        }

        constexpr const_iterator operator++(int) noexcept
        {
            auto tmp = *this;
            ++(*this);
            return tmp;
        }

        friend constexpr bool operator==(const const_iterator& a,
                                         const const_iterator& b) noexcept
        {
            return a._values_ptr == b._values_ptr &&
                   a._ptr == b._ptr;
        }

        friend constexpr bool operator!=(const const_iterator& a,
                                         const const_iterator& b) noexcept
        {
            return !(a == b);
        }
    };

    using iterator = const_iterator;

    [[nodiscard]] constexpr const_iterator begin() const noexcept
    {
        return const_iterator(this->_values.data(), 0);
    }

    [[nodiscard]] constexpr const_iterator end() const noexcept
    {
        return const_iterator(this->_values.data(), N);
    }

    [[nodiscard]] constexpr const_iterator cbegin() const noexcept
    {
        return this->begin();
    }

    [[nodiscard]] constexpr const_iterator cend() const noexcept
    {
        return this->end();
    }

    [[nodiscard]] constexpr const_iterator find(const Key& key) const noexcept
    {
        const std::size_t value_index = this->find_index(key);

        if(value_index == N)
            return this->end();

        return const_iterator(this->_values.data(), value_index);
    }

    [[nodiscard]] constexpr bool contains(const Key& key) const noexcept
    {
        return this->find_index(key) != N;
    }

    [[nodiscard]] constexpr std::size_t size() const noexcept
    {
        return N;
    }

    [[nodiscard]] constexpr std::size_t capacity() const noexcept
    {
        return TABLE_SIZE;
    }

    [[nodiscard]] constexpr bool empty() const noexcept
    {
        return false;
    }
};

ICY_NAMESPACE_END

#endif // !defined(__ICY)