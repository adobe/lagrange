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

#include <numeric>
#include <vector>

#include <lagrange/AttributeTypes.h>
#include <lagrange/Logger.h>
#include <lagrange/utils/DisjointSets.h>
#include <lagrange/utils/assert.h>
#include <lagrange/utils/invalid.h>
#include <lagrange/utils/range.h>
#include <lagrange/utils/safe_cast.h>
#include <lagrange/utils/span.h>

namespace lagrange {

template <typename IndexType>
void DisjointSets<IndexType>::init(size_t n)
{
    m_parent.resize(n);
    std::iota(m_parent.begin(), m_parent.end(), IndexType(0));
}

template <typename IndexType>
IndexType DisjointSets<IndexType>::find(IndexType i)
{
    la_runtime_assert(i >= 0 && i < safe_cast<IndexType>(m_parent.size()), "Index out of bound!");
    while (m_parent[i] != i) {
        m_parent[i] = m_parent[m_parent[i]];
        i = m_parent[i];
    }
    return i;
}

template <typename IndexType>
std::vector<std::vector<IndexType>> DisjointSets<IndexType>::extract_disjoint_sets()
{
    const auto num_entries = size();
    std::vector<IndexType> index_map;
    const auto counter = extract_disjoint_set_indices(index_map);

    std::vector<std::vector<IndexType>> disjoint_sets(counter);
    for (auto i : range(num_entries)) {
        disjoint_sets[index_map[i]].push_back(i);
    }
    return disjoint_sets;
}

template <typename IndexType>
size_t DisjointSets<IndexType>::extract_disjoint_set_indices(std::vector<IndexType>& index_map)
{
    const IndexType num_entries = size();
    index_map.resize(num_entries, invalid<IndexType>());
    return extract_disjoint_set_indices({index_map.data(), index_map.size()});
}

template <typename IndexType>
size_t DisjointSets<IndexType>::extract_disjoint_set_indices(span<IndexType> index_map)
{
    const size_t num_entries = size();
    la_runtime_assert(
        index_map.size() >= num_entries,
        fmt::format("Index map must be large enough to hold {} entries!", num_entries));
    constexpr IndexType invalid_index = invalid<IndexType>();
    std::fill(index_map.begin(), index_map.end(), invalid_index);
    IndexType counter = 0;

    // Assign each roots a unique index.
    for (auto i : range(num_entries)) {
        const auto root = find(i);
        if (static_cast<IndexType>(i) == root) index_map[i] = counter++;
    }

    // Assign all members the same index as their root.
    for (auto i : range(num_entries)) {
        const auto root = find(i);
        la_debug_assert(index_map[root] != invalid<IndexType>());
        index_map[i] = index_map[root];
    }

    return static_cast<size_t>(counter);
}

#define LA_X_DisjointSets(_, Index) template class DisjointSets<Index>;
LA_ATTRIBUTE_INDEX_X(DisjointSets, 0)

} // namespace lagrange
