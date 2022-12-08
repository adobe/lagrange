/*
 * Copyright 2021 Adobe. All rights reserved.
 * This file is licensed to you under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License. You may obtain a copy
 * of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under
 * the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
 * OF ANY KIND, either express or implied. See the License for the specific language
 * governing permissions and limitations under the License.
 */
#pragma once

#include <lagrange/utils/assert.h>

#include <algorithm>
#include <array>
#include <initializer_list>

namespace lagrange {

///
/// @ingroup    group-utils
///
/// @{

///
/// Stack-allocated set with a maximum size.
///
/// @tparam     T     Value type.
/// @tparam     N     Maximum size.
///
template <typename T, size_t N>
struct StackSet
{
private:
    std::array<T, N> m_array;
    size_t m_size = 0;

public:
    StackSet() = default;

    StackSet(std::initializer_list<T> init)
        : m_size(init.size())
    {
        la_runtime_assert(m_size <= N);
        auto it = init.begin();
        for (size_t i = 0; i < m_size; ++i) {
            m_array[i] = std::move(*it);
            ++it;
        }
        ensure_unique();
    }

public:
    using iterator = typename std::array<T, N>::iterator;
    using const_iterator = typename std::array<T, N>::const_iterator;
    iterator begin() { return m_array.begin(); }
    iterator end() { return m_array.begin() + m_size; }
    const_iterator begin() const { return m_array.begin(); }
    const_iterator end() const { return m_array.begin() + m_size; }

public:
    size_t size() const { return m_size; }

    void clear() { m_size = 0; }

    void resize(const size_t i)
    {
        la_runtime_assert(i <= m_array.size());
        m_size = i;
    }

    std::pair<iterator, bool> insert(const T& v)
    {
        la_runtime_assert(m_size < m_array.size());
        for (size_t i = 0; i < m_size; ++i) {
            if (m_array[i] == v) {
                return {begin() + i, false};
            }
        }
        m_array[m_size++] = v;
        return {begin() + m_size - 1, true};
    }

    size_t erase(const T& v)
    {
        la_runtime_assert(m_size < m_array.size());
        auto it = find(v);
        if (it != end()) {
            std::swap(*it, *(end() - 1));
            --m_size;
            return 1;
        }
        return 0;
    }

    bool contains(const T& v) const { return find(v) != end(); }

    const_iterator find(const T& v) const { return std::find(begin(), end(), v); }

    const T* data() const { return m_array.data(); }

    const T& front() const
    {
        la_runtime_assert(m_size > 0);
        return m_array.front();
    }

    const T& back() const
    {
        la_runtime_assert(m_size > 0);
        return m_array.at(m_size - 1);
    }

    const T& at(const size_t i) const
    {
        la_runtime_assert(i < m_size);
        return m_array.at(i);
    }

    const T& operator[](const size_t i) const
    {
        la_runtime_assert(i < m_size);
        return m_array[i];
    }

    template <typename U, class UnaryOperation>
    auto transformed(UnaryOperation op)
    {
        StackSet<U, N> result;
        result.resize(size());
        for (size_t i = 0; i < size(); ++i) {
            result[i] = op(at(i));
        }
        result.ensure_unique();
        return result;
    }

    template <size_t D>
    auto to_tuple()
    {
        assert(D == m_size);
        static_assert(D <= N, "Invalid size");
        return to_tuple_helper(std::make_index_sequence<D>());
    }

protected:
    template <size_t... Indices>
    auto to_tuple_helper(std::index_sequence<Indices...>)
    {
        return std::make_tuple(m_array[Indices]...);
    }

    void ensure_unique()
    {
        std::sort(m_array.begin(), m_array.end());
        auto it = std::unique(m_array.begin(), m_array.end());
        m_size = static_cast<size_t>(std::distance(m_array.begin(), it));
    }

    iterator find(const T& v) { return std::find(begin(), end(), v); }

    T* data() { return m_array.data(); }

    T& front()
    {
        la_runtime_assert(m_size > 0);
        return m_array.front();
    }

    T& back()
    {
        la_runtime_assert(m_size > 0);
        return m_array.at(m_size - 1);
    }
};

template <class T, size_t N>
bool operator==(const StackSet<T, N>& lhs, const StackSet<T, N>& rhs)
{
    return (lhs.size() == rhs.size() && std::equal(lhs.begin(), lhs.end(), rhs.begin()));
}

/// @}

} // namespace lagrange
