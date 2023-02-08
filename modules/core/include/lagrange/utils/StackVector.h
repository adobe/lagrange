/*
 * Copyright 2022 Adobe. All rights reserved.
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
/// Stack-allocated vector with a maximum size.
///
/// @tparam     T     Value type.
/// @tparam     N     Maximum size.
///
template <typename T, size_t N>
struct StackVector
{
private:
    std::array<T, N> m_array;
    size_t m_size = 0;

public:
    StackVector() = default;

    StackVector(std::initializer_list<T> init)
        : m_size(init.size())
    {
        la_runtime_assert(m_size <= N);
        auto it = init.begin();
        for (size_t i = 0; i < m_size; ++i) {
            m_array[i] = std::move(*it);
            ++it;
        }
    }

    size_t size() const { return m_size; }

    void clear() { m_size = 0; }

    void resize(const size_t i)
    {
        la_runtime_assert(i <= m_array.size());
        m_size = i;
    }

    void push_back(const T& v)
    {
        la_runtime_assert(m_size < m_array.size());
        m_array[m_size++] = v;
    }

    template <class... Args>
    void emplace_back(Args&&... args)
    {
        la_runtime_assert(m_size < m_array.size());
        m_array[m_size++] = T(std::forward<Args>(args)...);
    }

    void pop_back()
    {
        la_runtime_assert(m_size > 0);
        --m_size;
    }

    T* data() { return m_array.data(); }

    const T* data() const { return m_array.data(); }

    T& front()
    {
        la_runtime_assert(m_size > 0);
        return m_array.front();
    }

    const T& front() const
    {
        la_runtime_assert(m_size > 0);
        return m_array.front();
    }

    T& back()
    {
        la_runtime_assert(m_size > 0);
        return m_array.at(m_size - 1);
    }

    const T& back() const
    {
        la_runtime_assert(m_size > 0);
        return m_array.at(m_size - 1);
    }

    T& at(const size_t i)
    {
        la_runtime_assert(i < m_size);
        return m_array.at(i);
    }

    const T& at(const size_t i) const
    {
        la_runtime_assert(i < m_size);
        return m_array.at(i);
    }

    T& operator[](const size_t i)
    {
        la_runtime_assert(i < m_size);
        return m_array[i];
    }

    const T& operator[](const size_t i) const
    {
        la_runtime_assert(i < m_size);
        return m_array[i];
    }

    template <typename U, class UnaryOperation>
    auto transformed(UnaryOperation op) {
        StackVector<U, N> result;
        result.resize(size());
        for (size_t i = 0; i < size(); ++i) {
            result[i] = op(at(i));
        }
        return result;
    }

    template <size_t D>
    auto to_tuple() {
        la_debug_assert(D == m_size);
        static_assert(D <= N, "Invalid size");
        return to_tuple_helper(std::make_index_sequence<D>());
    }

private:
    template <size_t... Indices>
    auto to_tuple_helper(std::index_sequence<Indices...>) {
        return std::make_tuple(m_array[Indices]...);
    }

public:
    using iterator = typename std::array<T, N>::iterator;
    using const_iterator = typename std::array<T, N>::const_iterator;
    iterator begin() { return m_array.begin(); }
    iterator end() { return m_array.begin() + m_size; }
    const_iterator begin() const { return m_array.begin(); }
    const_iterator end() const { return m_array.begin() + m_size; }
};

template <class T, size_t N>
bool operator==(const StackVector<T, N>& lhs, const StackVector<T, N>& rhs)
{
    return (lhs.size() == rhs.size() && std::equal(lhs.begin(), lhs.end(), rhs.begin()));
}

/// @}

} // namespace lagrange
