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

#include <numeric>
#include <vector>

#include <lagrange/common.h>
#include <lagrange/utils/la_assert.h>
#include <lagrange/utils/range.h>
#include <lagrange/utils/safe_cast.h>

namespace lagrange {

template <typename IndexType>
class DisjointSets
{
public:
    DisjointSets() = default;
    explicit DisjointSets(IndexType n) { init(n); }

    void init(IndexType n)
    {
        m_parent.resize(n);
        std::iota(m_parent.begin(), m_parent.end(), IndexType(0));
    }

    IndexType size() const { return safe_cast<IndexType>(m_parent.size()); }

    void clear() { m_parent.clear(); }

    IndexType find(IndexType i)
    {
        LA_ASSERT(i >= 0 && i < safe_cast<IndexType>(m_parent.size()), "Index out of bound!");
        while (m_parent[i] != i) {
            m_parent[i] = m_parent[m_parent[i]];
            i = m_parent[i];
        }
        return i;
    }

    IndexType merge(IndexType i, IndexType j)
    {
        const auto root_i = find(i);
        const auto root_j = find(j);
        return m_parent[root_j] = root_i;
    }

    std::vector<std::vector<IndexType>> extract_disjoint_sets()
    {
        const IndexType num_entries = size();
        std::vector<IndexType> index_map;
        const IndexType counter = extract_disjoint_set_indices(index_map);

        std::vector<std::vector<IndexType>> disjoint_sets(counter);
        for (auto i : range(num_entries)) {
            disjoint_sets[index_map[i]].push_back(i);
        }
        return disjoint_sets;
    }

    /**
     * Assign all elements their disjoint set index. Each disjoint set index
     * ranges from 0 to k-1, where k is the number of disjoint sets.
     */
    IndexType extract_disjoint_set_indices(std::vector<IndexType>& index_map)
    {
        const IndexType num_entries = size();
        index_map.resize(num_entries, INVALID<IndexType>());
        IndexType counter = 0;

        // Assign each roots a unique index.
        for (auto i : range(num_entries)) {
            const auto root = find(i);
            if (i == root) {
                index_map[i] = counter;
                counter++;
            }
        }

        // Assign all members the same index as their root.
        for (auto i : range(num_entries)) {
            const auto root = find(i);
            assert(index_map[root] != INVALID<IndexType>());
            index_map[i] = index_map[root];
        }

        return counter;
    }

private:
    std::vector<IndexType> m_parent;
};

} // namespace lagrange
