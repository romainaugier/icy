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
#define ICY_MAX_BUILDING_ROUNDS 16
#endif // defined(ICY_MAX_BUILDING_ROUNDS)

#include <array>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <limits>
#include <optional>
#include <string_view>
#include <type_traits>
#include <utility>

ICY_NAMESPACE_BEGIN

DETAIL_NAMESPACE_BEGIN

constexpr std::uint32_t fnv1a(std::string_view sv) noexcept
{
    std::uint32_t h = 2166136261u;

    for(char c : sv)
    {
        h ^= static_cast<std::uint8_t>(c);
        h *= 16777619u;
    }

    return h;
}

constexpr std::uint32_t mix64(std::uint64_t x) noexcept
{
    x ^= x >> 33;
    x *= 0xff51afd7ed558ccdull;
    x ^= x >> 33;
    x *= 0xc4ceb9fe1a85ec53ull;
    x ^= x >> 33;

    return static_cast<std::uint32_t>(x);
}

constexpr std::uint32_t mix32(std::uint32_t x) noexcept
{
    x ^= x >> 16;
    x *= 0x85ebca6bu;
    x ^= x >> 13;
    x *= 0xc2b2ae35u;
    x ^= x >> 16;

    return x;
}

template <typename Key>
constexpr std::uint32_t hash_key(const Key& key) noexcept
{
    if constexpr(std::is_integral_v<Key>)
    {
        return mix64(static_cast<std::uint64_t>(key));
    }
    else
    {
        return fnv1a(std::string_view{key});
    }
}

template <typename Key>
constexpr std::uint32_t hash_key(const Key& key, std::uint32_t seed) noexcept
{
    return mix32(hash_key(key) ^ seed);
}

constexpr std::uint8_t fingerprint(std::uint32_t hash) noexcept
{
    std::uint8_t result = static_cast<std::uint8_t>(hash >> 24);

    if(result == 0)
        result = 1;

    return result;
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

    // Keep the load factor at or below 0.5.
    return next_pow2(n * 2);
}

template <typename Key>
constexpr bool equal(const Key& a, const Key& b) noexcept
{
    return a == b;
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

    using slot_type = std::optional<value_type>;
    using index_type = std::uint32_t;

    std::array<slot_type, N> _values{};
    std::array<index_type, TABLE_SIZE> _indices{};
    std::array<std::uint8_t, TABLE_SIZE> _fingerprints{};

    std::uint32_t _seed{0};

    struct build_result
    {
        std::array<index_type, TABLE_SIZE> indices{};
        std::array<std::uint8_t, TABLE_SIZE> fingerprints{};

        std::uint32_t seed{0};
        std::size_t total_probes{0};
        std::size_t max_probe{0};
        bool valid{false};
    };

    template <typename Container>
    static consteval build_result build(const Container& items)
    {
        build_result best{};

        best.indices.fill(EMPTY);

        for(std::size_t round = 0; round < ICY_MAX_BUILDING_ROUNDS; ++round)
        {
            build_result candidate{};

            candidate.indices.fill(EMPTY);
            candidate.seed = static_cast<std::uint32_t>(round);

            bool duplicate = false;

            for(std::size_t value_index = 0; value_index < N; ++value_index)
            {
                const auto& item = items[value_index];
                const auto& key = item.first;

                const std::uint32_t hash =
                    detail::hash_key(key, candidate.seed);

                const std::uint8_t fp =
                    detail::fingerprint(hash);

                std::size_t idx = hash & MASK;
                std::size_t probes = 0;

                while(candidate.indices[idx] != EMPTY)
                {
                    const std::size_t existing_index =
                        candidate.indices[idx];

                    if(detail::equal(items[existing_index].first, key))
                    {
                        duplicate = true;
                        break;
                    }

                    idx = (idx + 1) & MASK;
                    ++probes;
                }

                if(duplicate)
                    break;

                candidate.indices[idx] =
                    static_cast<index_type>(value_index);

                candidate.fingerprints[idx] = fp;
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
    consteval map(const Container& items)
    {
        static_assert(sizeof(items) / sizeof(items[0]) == N,
                      "icy::map: initializer size mismatch");

        const build_result result = build(items);

        this->_indices = result.indices;
        this->_fingerprints = result.fingerprints;
        this->_seed = result.seed;

        for(std::size_t i = 0; i < N; ++i)
            this->_values[i].emplace(items[i].first, items[i].second);
    }

    [[nodiscard]] constexpr std::size_t find_index(const Key& key) const noexcept
    {
        const std::uint32_t hash =
            detail::hash_key(key, this->_seed);

        const std::uint8_t fp =
            detail::fingerprint(hash);

        std::size_t idx = hash & MASK;

        while(this->_indices[idx] != EMPTY)
        {
            if(this->_fingerprints[idx] == fp)
            {
                const std::size_t value_index =
                    this->_indices[idx];

                if(this->_values[value_index]->first == key)
                    return value_index;
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

        const slot_type* _values_ptr{nullptr};
        std::size_t _ptr{0};

        constexpr const_iterator(const slot_type* values,
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
            return *this->_values_ptr[this->_ptr];
        }

        constexpr pointer operator->() const noexcept
        {
            return &*this->_values_ptr[this->_ptr];
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

    using slot_type = std::optional<value_type>;
    using index_type = std::uint32_t;

    std::array<slot_type, N> _values{};
    std::array<index_type, TABLE_SIZE> _indices{};
    std::array<std::uint8_t, TABLE_SIZE> _fingerprints{};

    std::uint32_t _seed{0};

    struct build_result
    {
        std::array<index_type, TABLE_SIZE> indices{};
        std::array<std::uint8_t, TABLE_SIZE> fingerprints{};

        std::uint32_t seed{0};
        std::size_t total_probes{0};
        std::size_t max_probe{0};
        bool valid{false};
    };

    template <typename Container>
    static consteval build_result build(const Container& items)
    {
        build_result best{};

        best.indices.fill(EMPTY);

        for(std::size_t round = 0; round < ICY_MAX_BUILDING_ROUNDS; ++round)
        {
            build_result candidate{};

            candidate.indices.fill(EMPTY);
            candidate.seed = static_cast<std::uint32_t>(round);

            bool duplicate = false;

            for(std::size_t value_index = 0; value_index < N; ++value_index)
            {
                const auto& key = items[value_index];

                const std::uint32_t hash =
                    detail::hash_key(key, candidate.seed);

                const std::uint8_t fp =
                    detail::fingerprint(hash);

                std::size_t idx = hash & MASK;
                std::size_t probes = 0;

                while(candidate.indices[idx] != EMPTY)
                {
                    const std::size_t existing_index =
                        candidate.indices[idx];

                    if(detail::equal(items[existing_index], key))
                    {
                        duplicate = true;
                        break;
                    }

                    idx = (idx + 1) & MASK;
                    ++probes;
                }

                if(duplicate)
                    break;

                candidate.indices[idx] =
                    static_cast<index_type>(value_index);

                candidate.fingerprints[idx] = fp;
                candidate.total_probes += probes;

                if(probes > candidate.max_probe)
                    candidate.max_probe = probes;
            }

            if(duplicate)
                throw "icy::set: duplicate key in initializer";

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
    consteval set(const Container& items)
    {
        static_assert(sizeof(items) / sizeof(items[0]) == N,
                      "icy::set: initializer size mismatch");

        const build_result result = build(items);

        this->_indices = result.indices;
        this->_fingerprints = result.fingerprints;
        this->_seed = result.seed;

        for(std::size_t i = 0; i < N; ++i)
            this->_values[i].emplace(items[i]);
    }

    [[nodiscard]] constexpr std::size_t find_index(const Key& key) const noexcept
    {
        const std::uint32_t hash =
            detail::hash_key(key, this->_seed);

        const std::uint8_t fp =
            detail::fingerprint(hash);

        std::size_t idx = hash & MASK;

        while(this->_indices[idx] != EMPTY)
        {
            if(this->_fingerprints[idx] == fp)
            {
                const std::size_t value_index =
                    this->_indices[idx];

                if(*this->_values[value_index] == key)
                    return value_index;
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

        const slot_type* _values_ptr{nullptr};
        std::size_t _ptr{0};

        constexpr const_iterator(const slot_type* values,
                                 std::size_t index) noexcept :
            _values_ptr(values),
            _ptr(index)
        {
        }

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
            return *this->_values_ptr[this->_ptr];
        }

        constexpr pointer operator->() const noexcept
        {
            return &*this->_values_ptr[this->_ptr];
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