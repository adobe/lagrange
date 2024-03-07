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

#include <vector>

#include <lagrange/utils/assert.h>
#include <lagrange/utils/span.h>

namespace lagrange {

///
/// @defgroup    group-utils
///
/// @{

/**
 * Adjacency list.
 *
 * @tparam Index  The index type.
 */
template <typename Index>
class AdjacencyList
{
public:
    using ValueArray = std::vector<Index>;
    using IndexArray = std::vector<size_t>;

public:
    /**
     * Initialize adjacency list data.
     *
     * @param data    The flattened adjacency list for all entries.
     *
     * @param indices The starting index of adjacency list for each entry.
     *                This list should have size `num_entries + 1`, where
     *                entry `i` is adjacent to entries from `m_data[i]` (included) to
     *                `m_data[i+1]`(not included).
     */
    AdjacencyList(ValueArray data, IndexArray indices)
        : m_data(std::move(data))
        , m_indices(std::move(indices))
    {}

    /**
     * Get total number of entries.
     *
     * @return The total number of entries.
     */
    [[nodiscard]] size_t get_num_entries() const { return m_indices.size() - 1; }

    /**
     * Get adjacency list of entry i.
     *
     * @param[in]  i  The query entry index.
     *
     * @return A list of adjacency entries.
     */
    [[nodiscard]] span<const Index> get_neighbors(size_t i) const
    {
        la_runtime_assert(
            i + 1 < m_indices.size(),
            "Invalid index, perhaps adjacency list is uninitialized?");
        return {m_data.data() + m_indices[i], m_indices[i + 1] - m_indices[i]};
    }

    /**
     * Get the number of neighbors of a given entry.
     *
     * @param i  The query entry.
     *
     * @return   The number of neighbors to i'th entry.
     */
    [[nodiscard]] size_t get_num_neighbors(size_t i) const
    {
        la_runtime_assert(
            i + 1 < m_indices.size(),
            "Invalid index, perhaps adjacency list is uninitialized?");
        return m_indices[i + 1] - m_indices[i];
    }

private:
    ValueArray m_data;
    IndexArray m_indices;
};

/// @}

} // namespace lagrange
