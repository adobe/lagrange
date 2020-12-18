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
#include <vector>

#include <lagrange/MeshGeometry.h>
#include <lagrange/utils/la_assert.h>

namespace lagrange {
template <typename GeometryType>
class Connectivity
{
public:
    using Index = typename GeometryType::Index;
    using IndexList = std::vector<Index>;
    using AdjacencyList = std::vector<IndexList>;

    Connectivity()
        : m_initialized(false)
    {}

    void initialize(const GeometryType& geometry)
    {
        m_v2v.clear();
        m_v2f.clear();
        m_f2f.clear();

        auto remove_duplicate_entries = [](IndexList& arr) {
            std::sort(arr.begin(), arr.end());
            const auto end_itr = std::unique(arr.begin(), arr.end());
            arr.resize(std::distance(arr.begin(), end_itr));
        };

        auto extract_duplicate_entries = [](IndexList& arr) {
            std::sort(arr.begin(), arr.end());
            IndexList duplicate_entries;
            Index curr = arr.back() + 1;
            for (const auto& item : arr) {
                if (curr == item &&
                    (duplicate_entries.empty() || item != duplicate_entries.back())) {
                    duplicate_entries.push_back(item);
                } else {
                    curr = item;
                }
            }
            arr.swap(duplicate_entries);
        };

        const Index num_vertices = geometry.get_num_vertices();
        const Index num_facets = geometry.get_num_facets();
        const Index vertex_per_facet = geometry.get_vertex_per_facet();
        m_v2v.resize(num_vertices);
        m_v2f.resize(num_vertices);
        m_f2f.resize(num_facets);
        for (auto& arr : m_v2v) arr.reserve(16);
        for (auto& arr : m_v2f) arr.reserve(16);
        for (auto& arr : m_f2f) arr.reserve(16);

        const auto& facets = geometry.get_facets();

        for (Index i = 0; i < num_facets; i++) {
            for (Index j = 0; j < vertex_per_facet; j++) {
                Index curr = facets(i, j);
                Index next = facets(i, (j + 1) % vertex_per_facet);
                Index prev = facets(i, (j + vertex_per_facet - 1) % vertex_per_facet);
                m_v2v[curr].push_back(next);
                m_v2v[curr].push_back(prev);
                m_v2f[curr].push_back(i);
            }
        }
        std::for_each(m_v2v.begin(), m_v2v.end(), remove_duplicate_entries);
        std::for_each(m_v2f.begin(), m_v2f.end(), remove_duplicate_entries);

        for (Index i = 0; i < num_facets; i++) {
            for (Index j = 0; j < vertex_per_facet; j++) {
                Index vi = facets(i, j);
                const auto& adj_facets = m_v2f[vi];
                m_f2f[i].insert(m_f2f[i].end(), adj_facets.begin(), adj_facets.end());
            }
        }
        std::for_each(m_f2f.begin(), m_f2f.end(), extract_duplicate_entries);

        // Remove self from f2f adjacency.
        for (Index i = 0; i < num_facets; i++) {
            const auto itr = std::find(m_f2f[i].begin(), m_f2f[i].end(), i);
            LA_ASSERT(itr != m_f2f[i].end());
            m_f2f[i].erase(itr);
        }

        m_initialized = true;
    }

    bool is_initialized() const { return m_initialized; }
    const AdjacencyList& get_vertex_vertex_adjacency() const { return m_v2v; }
    const AdjacencyList& get_vertex_facet_adjacency() const { return m_v2f; }
    const AdjacencyList& get_facet_facet_adjacency() const { return m_f2f; }
    const IndexList& get_vertices_adjacent_to_vertex(Index vi) const { return m_v2v[vi]; }
    const IndexList& get_facets_adjacent_to_vertex(Index vi) const { return m_v2f[vi]; }
    const IndexList& get_facets_adjacent_to_facet(Index fi) const { return m_f2f[fi]; }

protected:
    bool m_initialized;
    AdjacencyList m_v2v;
    AdjacencyList m_v2f;
    AdjacencyList m_f2f;
};

template <typename GeometryType>
std::unique_ptr<Connectivity<GeometryType>> compute_connectivity(const GeometryType& geometry)
{
    auto ptr = std::make_unique<Connectivity<GeometryType>>();

    ptr->initialize(geometry);

    return ptr;
}
} // namespace lagrange
