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
lagrange::sampling::internal::HeapWithHash<Key, Value>::HeapWithHash(RandomIt first, RandomIt last)
{
    m_container.reserve(std::distance(first, last));
    std::copy(first, last, std::back_inserter(m_container));
    std::make_heap(std::begin(m_container), std::end(m_container), CompareByValue());

    for (const auto& pair : m_container) {
        auto iter = m_min_values.find(pair.first);
        if (iter != std::cend(m_min_values)) {
            iter->second = std::min(iter->second, pair.second);
        } else {
            m_min_values.emplace(pair);
        }
    }
}

template <typename Key, typename Value>
bool lagrange::sampling::internal::HeapWithHash<Key, Value>::CompareByValue::operator()(
    const Pair& aa,
    const Pair& bb) const
{
    if (aa.second != bb.second) return aa.second < bb.second;
    return aa.first < bb.first;
}

template <typename Key, typename Value>
bool lagrange::sampling::internal::HeapWithHash<Key, Value>::insert_or_update(
    const Key& key,
    const Value& value)
{
    auto min_values_iter = m_min_values.find(key);

    if (min_values_iter == std::cend(m_min_values)) {
        m_container.emplace_back(Pair{key, value});
        std::push_heap(std::begin(m_container), std::end(m_container), CompareByValue());

        m_min_values.emplace(key, value);
        return true;
    }

    assert(min_values_iter != std::cend(m_min_values));
    const bool value_is_min = value < min_values_iter->second;
    if (value_is_min) {
        m_container.emplace_back(Pair{key, value});
        std::push_heap(std::begin(m_container), std::end(m_container), CompareByValue());

        min_values_iter->second = value;
    }

    return value_is_min;
}

template <typename Key, typename Value>
size_t lagrange::sampling::internal::HeapWithHash<Key, Value>::cleanup(const double factor)
{
    if (m_container.size() < static_cast<size_t>(m_min_values.size() * factor)) return 0;

    size_t counter = 0;

    m_tmp_container.clear();
    m_tmp_container.reserve(m_min_values.size());
    for (const auto& pair : m_container) {
        const auto min_values_iter = m_min_values.find(pair.first);
        assert(min_values_iter != std::cend(m_min_values));
        assert(pair.first == min_values_iter->first);

        const bool value_is_not_min = min_values_iter->second < pair.second;
        if (value_is_not_min) {
            counter++;
            continue;
        }

        m_tmp_container.emplace_back(pair);
    }

    if (counter) {
        std::swap(m_container, m_tmp_container);
        std::make_heap(std::begin(m_container), std::end(m_container), CompareByValue());
    }

    assert(m_container.size() >= m_min_values.size());


    return counter;
}

template <typename Key, typename Value>
typename lagrange::sampling::internal::HeapWithHash<Key, Value>::Pair
lagrange::sampling::internal::HeapWithHash<Key, Value>::pop()
{
    assert(!empty());

    std::pop_heap(std::begin(m_container), std::end(m_container), CompareByValue());
    Pair top = m_container.back();
    m_container.pop_back();

    typename KeyToMinValues::iterator min_values_iter = std::end(m_min_values);
    while (true) {
        min_values_iter = m_min_values.find(top.first);
        assert(min_values_iter != std::cend(m_min_values));

        const bool value_is_not_min = min_values_iter->second < top.second;
        if (!value_is_not_min) break;

        std::pop_heap(std::begin(m_container), std::end(m_container), CompareByValue());
        top = m_container.back();
        m_container.pop_back();
    }
    assert(min_values_iter != std::cend(m_min_values));

    m_min_values.erase(min_values_iter);

    return top;
}

template <typename Key, typename Value>
bool lagrange::sampling::internal::HeapWithHash<Key, Value>::empty() const
{
    assert(m_container.empty() == m_min_values.empty());
    return m_container.empty();
}

template <typename Key, typename Value>
void lagrange::sampling::internal::HeapWithHash<Key, Value>::reserve(const size_t new_cap)
{
    m_container.reserve(new_cap);
    m_min_values.reserve(new_cap);
}

template <typename Key, typename Value>
size_t lagrange::sampling::internal::HeapWithHash<Key, Value>::size() const
{
    assert(m_container.size() >= m_min_values.size());
    return m_min_values.size();
}

template <typename Key, typename Value>
size_t lagrange::sampling::internal::HeapWithHash<Key, Value>::heap_size() const
{
    assert(m_container.size() >= m_min_values.size());
    return m_container.size();
}

template <typename Key, typename Value>
bool lagrange::sampling::internal::HeapWithHash<Key, Value>::contains(const Key& key) const
{
    return m_min_values.find(key) != std::cend(m_min_values);
}

template <typename Key, typename Value>
const typename lagrange::sampling::internal::HeapWithHash<Key, Value>::Container&
lagrange::sampling::internal::HeapWithHash<Key, Value>::container() const
{
    return m_container;
}

template <typename Key, typename Value>
const typename lagrange::sampling::internal::HeapWithHash<Key, Value>::KeyToMinValues&
lagrange::sampling::internal::HeapWithHash<Key, Value>::min_values() const
{
    return m_min_values;
}
