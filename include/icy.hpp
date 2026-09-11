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

#include <array>
#include <cstddef>
#include <cstdint>
#include <iterator>
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

template <typename Key>
constexpr std::uint32_t hash_key(const Key& key) noexcept
{
    if constexpr (std::is_integral_v<Key>)
    {
        return mix64(static_cast<std::uint64_t>(key));
    }
    else 
    {
        return fnv1a(std::string_view{key});
    }
}

DETAIL_NAMESPACE_END

// icy::map

template<typename Key,
         typename Value,
         std::size_t N>
class map 
{
    static_assert(N >= 1, "icy::map requires at least one entry");

public:
    using key_type = Key;
    using mapped_type = Value;
    using value_type = std::pair<const Key, Value>;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    using reference = const value_type&;
    using pointer = const value_type*;

private:
    static constexpr std::size_t TABLE_SIZE = N * 2;

    using slot_type = std::optional<value_type>;

    std::array<slot_type, TABLE_SIZE> _table{};

    template <typename Container>
    consteval map(const Container& items) 
    {
        static_assert(std::size(items) == N, "icy::map: initializer size mismatch");

        for(const auto& [k, v] : items) 
        {
            std::size_t idx = detail::hash_key(k) % TABLE_SIZE;

            while(this->_table[idx].has_value()) 
            {
                if(this->_table[idx]->first == k)
                    throw "icy::map: duplicate key in initializer";

                idx = (idx + 1) % TABLE_SIZE;
            }

            this->_table[idx].emplace(k, v);
        }
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
        const slot_type* _table_ptr{nullptr};
        std::size_t _ptr{0};

        constexpr const_iterator(const slot_type* t, std::size_t i) noexcept : _table_ptr(t),
                                                                               _ptr(i)
        { 
            this->skip();
        }

        constexpr void skip() noexcept
        {
            while(this->_ptr < TABLE_SIZE && !this->_table_ptr[this->_ptr].has_value())
                ++this->_ptr;
        }

    public:
        using iterator_category = std::forward_iterator_tag;
        using iterator_concept = std::forward_iterator_tag;
        using value_type = map::value_type;
        using difference_type = std::ptrdiff_t;
        using pointer = const value_type*;
        using reference = const value_type&;

        const_iterator() = default;

        constexpr reference operator*() const noexcept { return *this->_table_ptr[this->_ptr]; }
        constexpr pointer operator->() const noexcept { return &*this->_table_ptr[this->_ptr]; }

        constexpr const_iterator& operator++() noexcept 
        {
            ++this->_ptr;
            this->skip();
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
            return a._table_ptr == b._table_ptr && a._ptr == b._ptr;
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
        return const_iterator(this->_table.data(), 0);
    }

    [[nodiscard]] constexpr const_iterator end() const noexcept 
    {
        return const_iterator(this->_table.data(), TABLE_SIZE);
    }

    [[nodiscard]] constexpr const_iterator cbegin() const noexcept { return this->begin(); }
    [[nodiscard]] constexpr const_iterator cend() const noexcept { return this->end(); }

    [[nodiscard]] constexpr const_iterator find(const Key& key) const noexcept
    {
        std::size_t idx = detail::hash_key(key) % TABLE_SIZE;

        while(this->_table[idx].has_value()) 
        {
            if(this->_table[idx]->first == key)
                return const_iterator(this->_table.data(), idx);

            idx = (idx + 1) % TABLE_SIZE;
        }

        return this->end();
    }

    [[nodiscard]] constexpr bool contains(const Key& key) const noexcept
    {
        return this->find(key) != this->end();
    }

    [[nodiscard]] constexpr std::size_t size() const noexcept { return N; }
    [[nodiscard]] constexpr std::size_t capacity() const noexcept { return TABLE_SIZE; }
    [[nodiscard]] constexpr bool empty() const noexcept { return false; }
};

// icy::set

template<typename Key,
         std::size_t N>
class set
{
    static_assert(N >= 1, "icy::set requires at least one element");

public:
    using key_type = Key;
    using value_type = Key;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    using reference = const value_type&;
    using pointer = const value_type*;

private:
    static constexpr std::size_t TABLE_SIZE = N * 2;

    using slot_type = std::optional<value_type>;

    std::array<slot_type, TABLE_SIZE> _table{};

    template <typename Container>
    consteval set(const Container& items)
    {
        static_assert(std::size(items) == N,
                      "icy::set: initializer size mismatch");

        for(const auto& k : items) 
        {
            std::size_t idx = detail::hash_key(k) % TABLE_SIZE;

            while(this->_table[idx].has_value())
            {
                if(*this->_table[idx] == k)
                    throw "icy::set: duplicate key in initializer";

                idx = (idx + 1) % TABLE_SIZE;
            }

            this->_table[idx].emplace(k);
        }
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
        const slot_type* _table_ptr{nullptr};
        std::size_t _ptr{0};

        constexpr const_iterator(const slot_type* t, std::size_t i) noexcept : _table_ptr(t),
                                                                               _ptr(i) 
        { 
            this->skip();
        }

        constexpr void skip() noexcept 
        {
            while(this->_ptr < TABLE_SIZE && !this->_table_ptr[_ptr].has_value())
                ++this->_ptr;
        }

    public:
        using iterator_category = std::forward_iterator_tag;
        using iterator_concept = std::forward_iterator_tag;
        using value_type = set::value_type;
        using difference_type = std::ptrdiff_t;
        using pointer = const value_type*;
        using reference = const value_type&;

        const_iterator() = default;

        constexpr reference operator*()  const noexcept { return *this->_table_ptr[this->_ptr]; }
        constexpr pointer operator->() const noexcept { return &*this->_table_ptr[this->_ptr]; }

        constexpr const_iterator& operator++() noexcept 
        {
            ++this->_ptr;
            this->skip();
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
            return a._table_ptr == b._table_ptr && a._ptr == b._ptr;
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
        return const_iterator(this->_table.data(), 0);
    }

    [[nodiscard]] constexpr const_iterator end() const noexcept 
    {
        return const_iterator(this->_table.data(), TABLE_SIZE);
    }

    [[nodiscard]] constexpr const_iterator cbegin() const noexcept { return this->begin(); }
    [[nodiscard]] constexpr const_iterator cend() const noexcept { return this->end(); }

    [[nodiscard]] constexpr const_iterator find(const Key& key) const noexcept
    {
        std::size_t idx = detail::hash_key(key) % TABLE_SIZE;

        while(this->_table[idx].has_value())
        {
            if(*this->_table[idx] == key)
                return const_iterator(this->_table.data(), idx);

            idx = (idx + 1) % TABLE_SIZE;
        }

        return end();
    }

    [[nodiscard]] constexpr bool contains(const Key& key) const noexcept
    {
        return this->find(key) != this->end();
    }

    [[nodiscard]] constexpr std::size_t size() const noexcept { return N; }
    [[nodiscard]] constexpr std::size_t capacity() const noexcept { return TABLE_SIZE; }
    [[nodiscard]] constexpr bool empty() const noexcept { return false; }
};

ICY_NAMESPACE_END

#endif // !defined(__ICY)