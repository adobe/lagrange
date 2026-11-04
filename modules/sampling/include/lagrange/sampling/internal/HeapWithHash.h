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

#include <algorithm>
#include <cassert>
#include <unordered_map>
#include <utility>
#include <vector>

namespace lagrange {
namespace sampling {
namespace internal {

///
/// HeapWithHash behaves like a maximum heap with the added functionality of being able to decrease the value of a key.
/// This is achieved by having an adjoint std::unordered_map that keeps track of minimum values.
///
/// @tparam Key Type of key. Must be ordered and hashable.
/// @tparam Value Type of value. Must be ordered.
template <typename Key, typename Value>
class HeapWithHash
{
public:
    using Pair = std::pair<Key, Value>;
    using Container = std::vector<Pair>;
    using KeyToMinValues = std::unordered_map<Key, Value>;

    /// Empty constructor.
    HeapWithHash() = default;

    /// Range constructor from pairs of key and value.
    ///
    /// @tparam RandomIt Type of random access iterator.
    /// @param[in] begin Beginning of the range.
    /// @param[in] end End of the range
    template <typename RandomIt>
    HeapWithHash(RandomIt begin, RandomIt end);

    /// Insert the pair (key, value) in the heap.
    /// If key has already inserted with an higher value, it is updated.
    ///
    /// @param key Key part of the inserted pair.
    /// @param value Value part of the inserted pair.
    /// @return Whether or not the insertion was minimal (e.g. was a new item or decreased recorded value).
    bool insert_or_update(const Key& key, const Value& value);

    /// Cleanup internal representation.
    /// Call once in a while if a lot a values have been updated to reduce memory footprint.
    ///
    /// @param[in] factor Ratio of heap size over size above which cleanup is triggered.
    /// @return Number of discarded values.
    size_t cleanup(const double factor);

    /// Pop maximum pair.
    /// This can only happen when the heap is not empty.
    ///
    /// @return Maximum pair.
    Pair pop();

    /// Check if the heap is empty.
    bool empty() const;

    /// Reserve space for new_cap elements.
    void reserve(const size_t new_cap);

    /// Number of registered keys.
    size_t size() const;

    /// Number of registered values.
    ///
    /// May be different from size if value of a specific key has been updated.
    /// If this is much larger than size, you may want to cleanup the heap.
    size_t heap_size() const;

    /// Check if heap contains key
    bool contains(const Key& key) const;

    /// Constant view of the heap.
    const Container& container() const;

    /// Constant view the key to minimum value map.
    const KeyToMinValues& min_values() const;

protected:
    struct CompareByValue
    {
        bool operator()(const Pair& aa, const Pair& bb) const;
    };

    Container m_container;
    Container m_tmp_container;
    KeyToMinValues m_min_values;
};

} // namespace internal
} // namespace sampling
} // namespace lagrange

#include <lagrange/sampling/internal/HeapWithHash.impl.h>
