/*
 * Copyright 2020 Adobe. All rights reserved.
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

#include <lagrange/common.h>

template <typename Key, typename Value>
template <typename RandomIt>
lagrange::sampling::internal::HeapWithVector<Key, Value>::HeapWithVector(
    RandomIt first,
    RandomIt last)
{
    constexpr auto default_value = std::numeric_limits<Value>::max();

    m_container.insert(m_container.end(), first, last);
    std::make_heap(std::begin(m_container), std::end(m_container), CompareByValue());

    Key max_key = 0;
    for (const auto& pair : m_container) {
        assert(pair.first >= 0);
        max_key = std::max(max_key, pair.first);
    }
    m_min_values.resize(max_key + 1, default_value);

    for (const auto& pair : m_container) {
        assert(pair.first < m_min_values.size());
        auto& val = m_min_values[pair.first];
        if (val != default_value) {
            assert(m_num_stale < m_container.size());
            m_num_stale++;
        }
        val = std::min(val, pair.second);
    }
}

template <typename Key, typename Value>
bool lagrange::sampling::internal::HeapWithVector<Key, Value>::CompareByValue::operator()(
    const Pair& aa,
    const Pair& bb) const
{
    if (aa.second != bb.second) return aa.second < bb.second;
    return aa.first < bb.first;
}

template <typename Key, typename Value>
bool lagrange::sampling::internal::HeapWithVector<Key, Value>::insert_or_update(
    const Key& key,
    const Value& value)
{
    constexpr auto default_value = std::numeric_limits<Value>::max();

    assert(key >= 0);
    assert(key < std::numeric_limits<Key>::max());
    if (static_cast<size_t>(key) >= m_min_values.size()) {
        m_min_values.resize(key + 1, default_value);
    }

    assert(static_cast<size_t>(key) < m_min_values.size());
    auto& min_val = m_min_values[key];
    const bool value_is_min = value < min_val;
    if (value_is_min) {
        if (min_val != default_value) {
            assert(m_num_stale < m_container.size());
            m_num_stale++;
        }

        m_container.emplace_back(Pair{key, value});
        std::push_heap(std::begin(m_container), std::end(m_container), CompareByValue());

        min_val = value;
    }

    return value_is_min;
}

template <typename Key, typename Value>
size_t lagrange::sampling::internal::HeapWithVector<Key, Value>::cleanup(const double factor)
{
#if !defined(NDEBUG)
    constexpr auto default_value = std::numeric_limits<Value>::max();
#endif
    if (static_cast<size_t>(m_container.size() + m_num_stale * factor) <
        static_cast<size_t>(m_container.size() * factor))
        return 0;

    size_t counter = 0;

    m_tmp_container.clear();
    m_tmp_container.reserve(m_container.size() - m_num_stale);
    for (const auto& pair : m_container) {
        const auto val = m_min_values[pair.first];
        assert(val != default_value);

        const bool value_is_not_min = val < pair.second;
        if (value_is_not_min) {
            counter++;
            continue;
        }

        m_tmp_container.emplace_back(pair);
    }

    assert(m_num_stale == counter);

    if (counter == 0) return 0;

    std::swap(m_container, m_tmp_container);
    std::make_heap(std::begin(m_container), std::end(m_container), CompareByValue());

    m_num_stale = 0;

    return counter;
}

template <typename Key, typename Value>
typename lagrange::sampling::internal::HeapWithVector<Key, Value>::Pair
lagrange::sampling::internal::HeapWithVector<Key, Value>::pop()
{
    constexpr auto default_value = std::numeric_limits<Value>::max();

    assert(!empty());

    std::pop_heap(std::begin(m_container), std::end(m_container), CompareByValue());
    Pair top = m_container.back();
    m_container.pop_back();

    while (true) {
        const auto min_val = m_min_values[top.first];

        const bool value_is_not_min = min_val < top.second;
        if (!value_is_not_min) break;

        std::pop_heap(std::begin(m_container), std::end(m_container), CompareByValue());
        top = m_container.back();
        m_container.pop_back();
        assert(m_num_stale > 0);
        --m_num_stale;
    }

    m_min_values[top.first] = default_value;

    return top;
}

template <typename Key, typename Value>
bool lagrange::sampling::internal::HeapWithVector<Key, Value>::empty() const
{
    return m_container.empty();
}

template <typename Key, typename Value>
void lagrange::sampling::internal::HeapWithVector<Key, Value>::reserve(const size_t new_cap)
{
    m_container.reserve(new_cap);
    m_min_values.reserve(new_cap);
}

template <typename Key, typename Value>
size_t lagrange::sampling::internal::HeapWithVector<Key, Value>::size() const
{
    assert(m_container.size() >= m_num_stale);
    return m_container.size() - m_num_stale;
}

template <typename Key, typename Value>
size_t lagrange::sampling::internal::HeapWithVector<Key, Value>::heap_size() const
{
    return m_container.size();
}

template <typename Key, typename Value>
bool lagrange::sampling::internal::HeapWithVector<Key, Value>::contains(const Key& key) const
{
    constexpr auto default_value = std::numeric_limits<Value>::max();
    if (key >= m_min_values.size()) return false;
    return m_min_values[key] != default_value;
}

template <typename Key, typename Value>
const typename lagrange::sampling::internal::HeapWithVector<Key, Value>::Container&
lagrange::sampling::internal::HeapWithVector<Key, Value>::container() const
{
    return m_container;
}

template <typename Key, typename Value>
const typename lagrange::sampling::internal::HeapWithVector<Key, Value>::KeyToMinValues&
lagrange::sampling::internal::HeapWithVector<Key, Value>::min_values() const
{
    return m_min_values;
}
