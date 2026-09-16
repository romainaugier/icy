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

// Per-bucket displacement search bound for perfect-hash construction
#if !defined(ICY_MAX_DISPLACEMENT_TRIES)
#define ICY_MAX_DISPLACEMENT_TRIES 4096
#endif // !defined(ICY_MAX_DISPLACEMENT_TRIES)

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <initializer_list>
#include <iterator>
#include <limits>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

ICY_NAMESPACE_BEGIN

DETAIL_NAMESPACE_BEGIN

// wyhash (final version 4.3), by Wang Yi <godspeed_china@yeah.net> et al.
// https://github.com/wangyi-fudan/wyhash - released into the public domain / Unlicense
// This is a constexpr, MSVC/GCC/Clang-portable adaptation using the default secret and a fixed external seed of 0

constexpr std::uint64_t wy_secret[4] = {
    0x2d358dccaa6c78a5ULL, 0x8bb84b93962eacc9ULL,
    0x4b33a62ed433d4a3ULL, 0x4d5a2da51de1aa47ULL
};

constexpr void wy_wymum(std::uint64_t& A, std::uint64_t& B) noexcept
{
    if consteval
    {
        const std::uint64_t ha = A >> 32, hb = B >> 32;
        const std::uint64_t la = static_cast<std::uint32_t>(A), lb = static_cast<std::uint32_t>(B);
        const std::uint64_t rh = ha * hb, rm0 = ha * lb, rm1 = hb * la, rl = la * lb;
        const std::uint64_t t = rl + (rm0 << 32);
        const std::uint64_t c = t < rl;
        const std::uint64_t lo = t + (rm1 << 32);
        const std::uint64_t c2 = c + (lo < t);
        const std::uint64_t hi = rh + (rm0 >> 32) + (rm1 >> 32) + c2;

        A = lo; B = hi;
    }
    else
    {
#if defined(__SIZEOF_INT128__)
        const __uint128_t r = static_cast<__uint128_t>(A) * B;

        A = static_cast<std::uint64_t>(r);
        B = static_cast<std::uint64_t>(r >> 64);
#elif defined(_MSC_VER) && defined(_M_X64)
        std::uint64_t hi;

        A = _umul128(A, B, &hi);
        B = hi;
#else
        const std::uint64_t ha = A >> 32, hb = B >> 32;
        const std::uint64_t la = static_cast<std::uint32_t>(A), lb = static_cast<std::uint32_t>(B);
        const std::uint64_t rh = ha * hb, rm0 = ha * lb, rm1 = hb * la, rl = la * lb;
        const std::uint64_t t = rl + (rm0 << 32);
        const std::uint64_t c = t < rl;
        const std::uint64_t lo = t + (rm1 << 32);
        const std::uint64_t c2 = c + (lo < t);
        const std::uint64_t hi2 = rh + (rm0 >> 32) + (rm1 >> 32) + c2;

        A = lo; B = hi2;
#endif
    }
}

constexpr std::uint64_t wy_wymix(std::uint64_t A, std::uint64_t B) noexcept
{
    wy_wymum(A, B);
    return A ^ B;
}

constexpr std::uint64_t wy_wyr8(const char* p) noexcept
{
    if consteval
    {
        std::uint64_t v = 0;

        for(int i = 0; i < 8; ++i)
            v |= static_cast<std::uint64_t>(static_cast<unsigned char>(p[i])) << (8 * i);

        return v;
    }

    std::uint64_t v;
    std::memcpy(std::addressof(v), p, 8);
    return v;
}

constexpr std::uint64_t wy_wyr4(const char* p) noexcept
{
    if consteval
    {
        std::uint32_t v = 0;

        for(int i = 0; i < 4; ++i)
            v |= static_cast<std::uint32_t>(static_cast<unsigned char>(p[i])) << (8 * i);

        return v;
    }

    std::uint32_t v;
    std::memcpy(std::addressof(v), p, 4);
    return v;
}

constexpr std::uint64_t wy_wyr3(const char* p, std::size_t k) noexcept
{
    return (static_cast<std::uint64_t>(static_cast<unsigned char>(p[0])) << 16) |
           (static_cast<std::uint64_t>(static_cast<unsigned char>(p[k >> 1])) << 8) | 
           static_cast<unsigned char>(p[k - 1]);
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

constexpr std::uint64_t mix64(std::uint64_t x) noexcept
{
    x ^= x >> 33;
    x *= 0xff51afd7ed558ccdull;
    x ^= x >> 33;
    x *= 0xc4ceb9fe1a85ec53ull;
    x ^= x >> 33;

    return x;
}

inline constexpr std::uint64_t wy_initial_seed = wy_wymix(wy_secret[0], wy_secret[1]);

constexpr std::uint64_t wyhash_base(const char* key, std::size_t len) noexcept
{
    const char* p = key;
    std::uint64_t seed = wy_initial_seed;
    std::uint64_t a, b;

    if(len <= 16)
    {
        if(len >= 4)
        {
            a = (wy_wyr4(p) << 32) | wy_wyr4(p + ((len >> 3) << 2));
            b = (wy_wyr4(p + len - 4) << 32) | wy_wyr4(p + len - 4 - ((len >> 3) << 2));
        }
        else if(len > 0)
        {
            a = wy_wyr3(p, len);
            b = 0;
        }
        else
        {
            a = b = 0;
        }
    }
    else
    {
        std::size_t i = len;

        if(i >= 48)
        {
            std::uint64_t see1 = seed, see2 = seed;

            do {
                seed = wy_wymix(wy_wyr8(p) ^ wy_secret[1], wy_wyr8(p + 8) ^ seed);
                see1 = wy_wymix(wy_wyr8(p + 16) ^ wy_secret[2], wy_wyr8(p + 24) ^ see1);
                see2 = wy_wymix(wy_wyr8(p + 32) ^ wy_secret[3], wy_wyr8(p + 40) ^ see2);
                p += 48;
                i -= 48;
            } while(i >= 48);

            seed ^= see1 ^ see2;
        }

        while(i > 16)
        {
            seed = wy_wymix(wy_wyr8(p) ^ wy_secret[1], wy_wyr8(p + 8) ^ seed);
            i -= 16;
            p += 16;
        }

        a = wy_wyr8(p + i - 16);
        b = wy_wyr8(p + i - 8);
    }

    a ^= wy_secret[1];
    b ^= seed;
    wy_wymum(a, b);

    return wy_wymix(a ^ wy_secret[0] ^ len, b ^ wy_secret[1]);
}

template <typename Key>
constexpr std::uint64_t hash_key_base(const Key& key) noexcept
{
    if constexpr (std::is_integral_v<Key>)
    {
        return static_cast<std::uint64_t>(key);
    }
    else
    {
        return wyhash_base(key.data(), key.size());
    }
}

template <typename Key>
constexpr std::uint64_t hash_finalize(std::uint64_t base, std::uint64_t seed) noexcept
{
    if constexpr (std::is_integral_v<Key>)
    {
        return (base ^ (seed * 0x9E3779B97F4A7C15ull)) * 0x9E3779B97F4A7C15ull;
    }
    else
    {
        return hash_len_16(base, seed);
    }
}

template <typename Key>
constexpr std::uint64_t premix_bucket_seed(std::uint64_t seed) noexcept
{
    if constexpr (std::is_integral_v<Key>)
    {
        return mix64(seed);
    }
    else
    {
        return seed * 0x9E3779B97F4A7C15ull;
    }
}

template <typename Key>
constexpr std::uint64_t bucket_hash(std::uint64_t base, std::uint64_t premix) noexcept
{
    if constexpr (std::is_integral_v<Key>)
    {
        return (base ^ premix) * 0x9E3779B97F4A7C15ull;
    }
    else
    {
        return mix64(base ^ premix);
    }
}

template <typename Key>
constexpr std::size_t hash_to_index(std::uint64_t hash, std::size_t mask, std::size_t shift) noexcept
{
    if constexpr (std::is_integral_v<Key>)
    {
        return static_cast<std::size_t>(hash >> shift);
    }
    else
    {
        return static_cast<std::size_t>(hash) & mask;
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

template <typename Key>
constexpr std::size_t table_size_for(std::size_t n) noexcept
{
    if(n == 0)
        return 1;

    if constexpr(std::is_integral_v<Key>)
    {
        if(n <= 64)
            return next_pow2(n * 16 + 1);

        return next_pow2(n * 2 + 1);
    }
    else
    {
        return next_pow2(n + n / 4 + 1);
    }
}

using index_type = std::uint32_t;

inline constexpr std::uint32_t EMPTY = std::numeric_limits<std::uint32_t>::max();

static_assert(ICY_MAX_DISPLACEMENT_TRIES < std::numeric_limits<std::uint16_t>::max(),
              "ICY_MAX_DISPLACEMENT_TRIES must fit in 16 bits (displacement is stored as uint16_t)");

inline constexpr std::uint16_t SLOT_EMPTY = 0;
inline constexpr std::uint16_t DIRECT = std::numeric_limits<std::uint16_t>::max();

struct slot
{
    std::uint64_t hash{0};
    index_type index{EMPTY};
    std::uint16_t displacement{SLOT_EMPTY};
};

template <typename Key>
struct compact_slot
{
    index_type index{EMPTY};
    std::uint16_t displacement{SLOT_EMPTY};
};

template <typename Key>
using slot_for = std::conditional_t<std::is_integral_v<Key>, compact_slot<Key>, slot>;

// Meta encoding used by icy::set slots:
//   bit 15 (SET_META_HAS_KEY) : a key is stored in this slot
//   bits 0-14                 : displacement code
//                               0x0000         : no bucket here
//                               0x0001..0x0FFE : indirect with d = code - 1
//                               0x7FFF         : direct (key sits at bucket position)
inline constexpr std::uint16_t SET_META_HAS_KEY = 0x8000;
inline constexpr std::uint16_t SET_META_DISP_MASK = 0x7FFF;
inline constexpr std::uint16_t SET_META_DISP_NONE = 0x0000;
inline constexpr std::uint16_t SET_META_DISP_DIRECT = 0x7FFF;

static_assert(ICY_MAX_DISPLACEMENT_TRIES < SET_META_DISP_DIRECT,
              "ICY_MAX_DISPLACEMENT_TRIES must fit in 15 bits for the set's meta encoding");

template <typename Key>
struct set_slot
{
    Key key{};
    index_type value_index{EMPTY};
    std::uint16_t meta{0};
};

template <typename Key>
struct hashed_set_slot
{
    Key key{};
    std::uint64_t hash{0};
    index_type value_index{EMPTY};
    std::uint16_t meta{0};
};

template <typename Key>
using set_slot_for = std::conditional_t<
    std::is_integral_v<Key>,
    set_slot<Key>,
    hashed_set_slot<Key>
>;

template <typename Key, std::size_t N, std::size_t M>
struct chd_result
{
    std::array<slot_for<Key>, M> table{};
    std::uint64_t bucket_seed{0};
    bool valid{false};
};

template <typename Key, std::size_t N, std::size_t M>
consteval chd_result<Key, N, M> build_perfect_hash(const std::array<Key, N>& keys)
{
    static_assert(M > 0 && (M & (M - 1)) == 0, "M must be a power of two");

    constexpr std::size_t MASK = M - 1;
    constexpr std::size_t SHIFT = 64 - std::countr_zero(M);

    std::array<std::uint64_t, N> key_base{};

    for(std::size_t i = 0; i < N; ++i)
        key_base[i] = hash_key_base(keys[i]);

    for(std::size_t round = 0; round < ICY_MAX_BUILDING_ROUNDS; ++round)
    {
        const std::uint64_t bucket_seed = static_cast<std::uint64_t>(round);
        const std::uint64_t bucket_premix = premix_bucket_seed<Key>(bucket_seed);

        std::array<std::size_t, N> bucket_of{};
        std::array<std::size_t, M> bucket_count{};

        for(std::size_t i = 0; i < N; ++i)
        {
            const std::size_t b = hash_to_index<Key>(bucket_hash<Key>(key_base[i], bucket_premix), MASK, SHIFT);
            bucket_of[i] = b;
            ++bucket_count[b];
        }

        std::array<std::size_t, M + 1> bucket_offset{};

        for(std::size_t b = 0; b < M; ++b)
            bucket_offset[b + 1] = bucket_offset[b] + bucket_count[b];

        std::array<std::size_t, M> cursor{};

        for(std::size_t b = 0; b < M; ++b)
            cursor[b] = bucket_offset[b];

        std::array<std::size_t, N> sorted_indices{};

        for(std::size_t i = 0; i < N; ++i)
            sorted_indices[cursor[bucket_of[i]]++] = i;

        std::array<std::size_t, M> bucket_order{};

        for(std::size_t b = 0; b < M; ++b)
            bucket_order[b] = b;

        std::sort(bucket_order.begin(), bucket_order.end(),
                  [&](std::size_t a, std::size_t b) noexcept
                  {
                      if(bucket_count[a] != bucket_count[b])
                          return bucket_count[a] > bucket_count[b];

                      return a < b;
                  });

        chd_result<Key, N, M> candidate{};

        candidate.bucket_seed = bucket_seed;

        bool duplicate = false;
        bool round_ok = true;

        for(std::size_t bi = 0; bi < M && round_ok; ++bi)
        {
            const std::size_t b = bucket_order[bi];
            const std::size_t count = bucket_count[b];

            if(count == 0)
                break;

            const std::size_t off = bucket_offset[b];

            for(std::size_t j = 0; j < count && !duplicate; ++j)
                for(std::size_t k = j + 1; k < count; ++k)
                    if(keys[sorted_indices[off + j]] == keys[sorted_indices[off + k]])
                    {
                        duplicate = true;
                        break;
                    }

            if(duplicate)
                break;

            bool placed = false;

            if(count == 1 && candidate.table[b].index == EMPTY)
            {
                const std::size_t idx = sorted_indices[off];

                candidate.table[b].index = static_cast<index_type>(idx);

                if constexpr(!std::is_integral_v<Key>)
                    candidate.table[b].hash = key_base[idx];

                candidate.table[b].displacement = DIRECT;

                placed = true;
            }

            for(std::size_t d = 0; d < ICY_MAX_DISPLACEMENT_TRIES && !placed; ++d)
            {
                std::vector<std::size_t> slots(count);
                std::vector<std::uint64_t> hashes(count);

                bool collision = false;

                for(std::size_t j = 0; j < count; ++j)
                {
                    const std::uint64_t h = hash_finalize<Key>(key_base[sorted_indices[off + j]], static_cast<std::uint64_t>(d));
                    const std::size_t s = hash_to_index<Key>(h, MASK, SHIFT);

                    hashes[j] = h;
                    slots[j] = s;

                    for(std::size_t k = 0; k < j; ++k)
                    {
                        if(slots[k] == s)
                        {
                            collision = true;
                            break;
                        }
                    }

                    if(collision)
                        break;

                    if(candidate.table[s].index != EMPTY)
                    {
                        collision = true;
                        break;
                    }
                }

                if(collision)
                    continue;

                for(std::size_t j = 0; j < count; ++j)
                {
                    const std::size_t idx = sorted_indices[off + j];

                    candidate.table[slots[j]].index = static_cast<index_type>(idx);

                    if constexpr(!std::is_integral_v<Key>)
                        candidate.table[slots[j]].hash = hashes[j];
                }

                // 1 + d, so 0 stays reserved for SLOT_EMPTY.
                candidate.table[b].displacement = static_cast<std::uint16_t>(d + 1);

                placed = true;
            }

            if(!placed)
                round_ok = false;
        }

        if(duplicate)
            throw "icy: duplicate key in initializer";

        if(!round_ok)
            continue;

        candidate.valid = true;

        return candidate;
    }

    return chd_result<Key, N, M>{};
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
    static constexpr std::size_t TABLE_SIZE = detail::table_size_for<Key>(N);
    static constexpr std::size_t MASK = TABLE_SIZE - 1;
    static constexpr std::size_t SHIFT = 64 - std::countr_zero(TABLE_SIZE);

    std::array<value_type, N> _values;
    std::array<detail::slot_for<Key>, TABLE_SIZE> _table{};

    std::uint64_t _bucket_seed{0};
    std::uint64_t _bucket_seed_premix{0};

    template <typename Container>
    static consteval std::array<value_type, N> make_values(const Container& items)
    {
        std::array<value_type, N> values{};

        for(std::size_t i = 0; i < N; ++i)
            std::construct_at(std::addressof(values[i]), items[i].first, items[i].second);

        return values;
    }

    template <typename Container>
    static consteval std::array<Key, N> make_keys(const Container& items)
    {
        std::array<Key, N> keys{};

        for(std::size_t i = 0; i < N; ++i)
            keys[i] = items[i].first;

        return keys;
    }

    template <typename Container>
    consteval map(const Container& items) : _values(make_values(items))
    {
        static_assert(sizeof(items) / sizeof(items[0]) == N,
                      "icy::map: initializer size mismatch");

        const auto result = detail::build_perfect_hash<Key, N, TABLE_SIZE>(make_keys(items));

        if(!result.valid)
            throw "icy::map: could not build a perfect hash within ICY_MAX_BUILDING_ROUNDS rounds";

        this->_table = result.table;
        this->_bucket_seed = result.bucket_seed;
        this->_bucket_seed_premix = detail::premix_bucket_seed<Key>(result.bucket_seed);
    }

    [[nodiscard]] constexpr std::size_t find_index(const Key& key) const noexcept
    {
        const std::uint64_t base = detail::hash_key_base(key);

        const std::size_t bucket = detail::hash_to_index<Key>(
            detail::bucket_hash<Key>(base, this->_bucket_seed_premix), MASK, SHIFT);

        const auto& entry = this->_table[bucket];

        if(entry.displacement == detail::SLOT_EMPTY)
            return N;

        if(entry.displacement == detail::DIRECT)
        {
            if(entry.index == detail::EMPTY)
                return N;

            if constexpr(std::is_integral_v<Key>)
            {
                if(this->_values[entry.index].first == key)
                    return entry.index;
            }
            else
            {
                if(entry.hash == base)
                {
#if defined(ICY_OPTIMIZE_KEY_CMP)
                    return entry.index;
#else
                    if(this->_values[entry.index].first == key)
                        return entry.index;
#endif // defined(ICY_OPTIMIZE_KEY_CMP)
                }
            }

            return N;
        }

        const std::uint64_t d = static_cast<std::uint64_t>(entry.displacement) - 1;
        const std::uint64_t hash = detail::hash_finalize<Key>(base, d);
        const std::size_t slot_index = detail::hash_to_index<Key>(hash, MASK, SHIFT);

        const auto& current = this->_table[slot_index];

        if(current.index == detail::EMPTY)
            return N;

        if constexpr(std::is_integral_v<Key>)
        {
            if(this->_values[current.index].first == key)
                return current.index;
        }
        else if(current.hash == hash)
        {
#if defined(ICY_OPTIMIZE_KEY_CMP)
            return current.index;
#else
            if(this->_values[current.index].first == key)
                return current.index;
#endif // defined(ICY_OPTIMIZE_KEY_CMP)
        }

        return N;
    }

public:
    static consteval map make(std::initializer_list<std::pair<Key, Value>> items)
    {
        if(items.size() != N)
            throw "icy::map: initializer has a different number of entries than N";

        std::array<std::pair<Key, Value>, N> array{};

        std::size_t i = 0;

        for(const auto& item : items)
            array[i++] = item;

        return map(array);
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
    static constexpr std::size_t TABLE_SIZE = detail::table_size_for<Key>(N);
    static constexpr std::size_t MASK = TABLE_SIZE - 1;
    static constexpr std::size_t SHIFT = 64 - std::countr_zero(TABLE_SIZE);

    using slot_type = detail::set_slot_for<Key>;

    std::array<value_type, N> _values;
    std::array<slot_type, TABLE_SIZE> _table{};
    std::uint64_t _bucket_seed{0};
    std::uint64_t _bucket_seed_premix{0};

    template <typename Container>
    static consteval std::array<value_type, N> make_values(const Container& items)
    {
        std::array<value_type, N> values{};

        for(std::size_t i = 0; i < N; ++i)
            values[i] = items[i];

        return values;
    }

    template <typename Container>
    consteval set(const Container& items) : _values(make_values(items))
    {
        static_assert(sizeof(items) / sizeof(items[0]) == N,
                      "icy::set: initializer size mismatch");

        const auto result = detail::build_perfect_hash<Key, N, TABLE_SIZE>(this->_values);

        if(!result.valid)
            throw "icy::set: could not build a perfect hash within ICY_MAX_BUILDING_ROUNDS rounds";

        this->_bucket_seed = result.bucket_seed;
        this->_bucket_seed_premix = detail::premix_bucket_seed<Key>(result.bucket_seed);

        for(std::size_t i = 0; i < TABLE_SIZE; ++i)
        {
            const auto& src = result.table[i];
            auto& dst = this->_table[i];

            std::uint16_t meta = detail::SET_META_DISP_NONE;

            if(src.index != detail::EMPTY)
            {
                meta |= detail::SET_META_HAS_KEY;
                dst.key = this->_values[src.index];
                dst.value_index = src.index;

                if constexpr(!std::is_integral_v<Key>)
                    dst.hash = src.hash;
            }
            else
            {
                dst.value_index = detail::EMPTY;
            }

            if(src.displacement == detail::DIRECT)
            {
                meta |= detail::SET_META_DISP_DIRECT;
            }
            else if(src.displacement != detail::SLOT_EMPTY)
            {
                meta |= src.displacement;
            }

            dst.meta = meta;
        }
    }

    [[nodiscard]] constexpr std::size_t find_index(const Key& key) const noexcept
    {
        const std::uint64_t base = detail::hash_key_base(key);

        const std::size_t bucket = detail::hash_to_index<Key>(
            detail::bucket_hash<Key>(base, this->_bucket_seed_premix), MASK, SHIFT);

        const auto& entry = this->_table[bucket];
        const std::uint16_t disp = entry.meta & detail::SET_META_DISP_MASK;

        if(disp == detail::SET_META_DISP_NONE)
            return N;

        if(disp == detail::SET_META_DISP_DIRECT)
        {
            if constexpr(std::is_integral_v<Key>)
            {
                if(entry.key == key)
                    return entry.value_index;
            }
            else
            {
                if(entry.hash == base)
                {
#if defined(ICY_OPTIMIZE_KEY_CMP)
                    return entry.value_index;
#else
                    if(entry.key == key)
                        return entry.value_index;
#endif // defined(ICY_OPTIMIZE_KEY_CMP)
                }
            }

            return N;
        }

        const std::uint64_t d = static_cast<std::uint64_t>(disp) - 1;
        const std::uint64_t hash = detail::hash_finalize<Key>(base, d);
        const std::size_t slot_index = detail::hash_to_index<Key>(hash, MASK, SHIFT);

        const auto& current = this->_table[slot_index];

        if((current.meta & detail::SET_META_HAS_KEY) == 0)
            return N;

        if constexpr(std::is_integral_v<Key>)
        {
            if(current.key == key)
                return current.value_index;
        }
        else if(current.hash == hash)
        {
#if defined(ICY_OPTIMIZE_KEY_CMP)
            return current.value_index;
#else
            if(current.key == key)
                return current.value_index;
#endif // defined(ICY_OPTIMIZE_KEY_CMP)
        }

        return N;
    }

public:
    static consteval set make(std::initializer_list<Key> items)
    {
        if(items.size() != N)
            throw "icy::set: initializer has a different number of entries than N";

        std::array<Key, N> array{};

        std::size_t i = 0;

        for(const auto& item : items)
            array[i++] = item;

        return set(array);
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