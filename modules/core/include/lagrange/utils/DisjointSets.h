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

#include <lagrange/utils/span.h>

#include <vector>

namespace lagrange {

///
/// @ingroup    group-utils
///
/// @{

/**
 * Disjoint sets computation.
 *
 * @tparam IndexType  Index type.
 */
template <typename IndexType>
class DisjointSets
{
public:
    /**
     * Initialize an empty disjoint sets.
     */
    DisjointSets() = default;

    /**
     * Initialize disjoint sets that contains `n` entries.
     *
     * @param[in]  n  The number of entries.
     */
    explicit DisjointSets(size_t n) { init(n); }

    /**
     * Initialize disjoint sets that contains `n` entries.
     *
     * @param[in]  n  The number of entries.
     */
    void init(size_t n);

    /**
     * Get the number of entries in total.
     */
    size_t size() const { return m_parent.size(); }

    /**
     * Clear all entries in the disjoint sets.
     */
    void clear()
    {
        m_parent.clear();
    }

    /**
     * Find the root index corresponding to index `i`.
     *
     * @param[in]  i  The qurey index.
     *
     * @return The root index that is in the same disjoint set as entry `i`.
     */
    IndexType find(IndexType i);

    /**
     * Merge the disjoint set containing entry `i` and the disjoint set containing entry `j`.
     *
     * @param[in] i  Entry index i.
     * @param[in] i  Entry index j.
     *
     * @return The root entry index of the merged set.
     */
    IndexType merge(IndexType i, IndexType j)
    {
        const auto root_i = find(i);
        const auto root_j = find(j);
        return m_parent[root_j] = root_i;
    }

    /**
     * Extract disjoint sets.
     *
     * @return A vector of disjoint sets.
     *
     * @deprecated This function is deprecated and may be removed in a future version.
     */
    [[deprecated]]
    std::vector<std::vector<IndexType>> extract_disjoint_sets();

    /**
     * Assign all elements their disjoint set index. Each disjoint set index
     * ranges from 0 to k-1, where k is the number of disjoint sets.
     *
     * @param[out]  index_map  The result buffer to hold the index.
     *
     * @return The number of disjoint sets.
     */
    size_t extract_disjoint_set_indices(std::vector<IndexType>& index_map);

    /**
     * Assign all elements their disjoint set index. Each disjoint set index
     * ranges from 0 to k-1, where k is the number of disjoint sets.
     *
     * @param[out]  index_map  The result buffer to hold the index.
     *
     * @return The number of disjoint sets.
     */
    size_t extract_disjoint_set_indices(span<IndexType> index_map);

private:
    std::vector<IndexType> m_parent;
};

/// @}

} // namespace lagrange
