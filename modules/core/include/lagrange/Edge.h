/*
 * Copyright 2016 Adobe. All rights reserved.
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

#include <array>
#include <exception>
#include <functional>
#include <memory>
#include <unordered_map>
#include <unordered_set>

#include <lagrange/common.h>
#include <lagrange/utils/assert.h>
#include <lagrange/utils/safe_cast.h>

namespace lagrange {

template <typename Index>
class EdgeType
{
private:
    Index m_v1;
    Index m_v2;

public:
    EdgeType(Index v1, Index v2)
        : m_v1(v1)
        , m_v2(v2)
    {}
    EdgeType(const EdgeType<Index>& obj)
        : m_v1(obj.v1())
        , m_v2(obj.v2())
    {}
    EdgeType(EdgeType<Index>&& obj) noexcept
        : m_v1(obj.v1())
        , m_v2(obj.v2())
    {}

    // Invalid() can be useful for EdgeType temporary variables
    static EdgeType Invalid() { return EdgeType(invalid<Index>(), invalid<Index>()); }

    // Alternate constructor for backward compatibility,
    // allowing `EdgeType e = {0, 1}`
    EdgeType(const std::array<Index, 2>& arr)
        : m_v1(arr[0])
        , m_v2(arr[1])
    {}

    inline Index v1() const { return m_v1; }
    inline Index v2() const { return m_v2; }
    inline bool is_valid() const { return m_v1 != invalid<Index>() && m_v2 != invalid<Index>(); }

    EdgeType<Index>& operator=(const EdgeType<Index>& other)
    {
        this->m_v1 = other.m_v1;
        this->m_v2 = other.m_v2;
        return *this;
    }
    EdgeType<Index>& operator=(EdgeType<Index>&& other) noexcept
    {
        this->m_v1 = other.m_v1;
        this->m_v2 = other.m_v2;
        return *this;
    }

    ~EdgeType() noexcept = default;

    bool operator==(const EdgeType<Index>& rhs) const
    {
        return (m_v1 == rhs.m_v1 && m_v2 == rhs.m_v2) || (m_v1 == rhs.m_v2 && m_v2 == rhs.m_v1);
    }
    bool operator!=(const EdgeType<Index>& rhs) const { return !(*this == rhs); }

    Index operator[](const Index i) const
    {
        if (i == 0) return m_v1;
        if (i == 1) return m_v2;
        la_runtime_assert(false, "Bad Index: " + std::to_string(i));
        return m_v1;
    }

    // Note: strict order operator< is NOT implemented, because:
    // - order of edges is not well defined
    // - by not implementing it, defining a std::set<EdgeType> will
    //   result in a compile error, and you should *not* use
    //   std::set<EdgeType>. Use std::unordered_set<EdgeType> instead.

    // allows: for (Index v : edge) { ... }
    class iterator : std::iterator<std::input_iterator_tag, EdgeType<Index>>
    {
    private:
        int m_i;
        const EdgeType<Index>& m_edge;

    public:
        iterator(const EdgeType<Index>& edge, int i)
            : m_i(i)
            , m_edge(edge)
        {}
        bool operator!=(const iterator& rhs) const
        {
            return m_edge != rhs.m_edge || m_i != rhs.m_i;
        }
        iterator& operator++()
        {
            ++m_i;
            return *this;
        }
        Index operator*() const { return m_edge[m_i]; }
    };
    iterator begin() const { return iterator(*this, 0); }
    iterator end() const { return iterator(*this, 2); }


    bool has_shared_vertex(const EdgeType<Index>& other) const
    {
        return m_v1 == other.m_v1 || m_v1 == other.m_v2 || m_v2 == other.m_v1 || m_v2 == other.m_v2;
    }
    Index get_shared_vertex(const EdgeType<Index>& other) const
    {
        la_runtime_assert(*this != other, "get_shared_vertex() failed due to identical edges");
        if (m_v1 == other.m_v1) return m_v1;
        if (m_v1 == other.m_v2) return m_v1;
        if (m_v2 == other.m_v1) return m_v2;
        if (m_v2 == other.m_v2) return m_v2;
        return invalid<Index>();
    }
    Index get_other_vertex(Index v) const
    {
        la_runtime_assert(m_v1 == v || m_v2 == v);
        if (m_v1 == v) return m_v2;
        return m_v1;
    }
};

template <typename Index, typename T>
using EdgeMap = std::unordered_map<EdgeType<Index>, T>;

template <typename Index>
using EdgeSet = std::unordered_set<EdgeType<Index>>;

template <typename MeshType>
using EdgeFacetMap =
    std::unordered_map<EdgeType<typename MeshType::Index>, std::vector<typename MeshType::Index>>;

// Computes a mapping from each edge to its adjacent facets.
// Only considers the sub-mesh defined by 'active_facets'
template <typename MeshType>
EdgeFacetMap<MeshType> compute_edge_facet_map_in_active_facets(
    const MeshType& mesh,
    const std::unordered_set<typename MeshType::Index>& active_facets)
{
    using Index = typename MeshType::Index;
    std::unordered_map<EdgeType<Index>, std::vector<Index>> edge_facet_map;
    const Index num_facets = mesh.get_num_facets();
    const Index vertex_per_facet = mesh.get_vertex_per_facet();
    const typename MeshType::FacetArray& facets = mesh.get_facets();
    edge_facet_map.reserve(active_facets.size() * vertex_per_facet);
    for (Index i = 0; i < num_facets; ++i) {
        if (active_facets.find(i) == active_facets.end()) continue;
        for (Index j = 0; j < vertex_per_facet; ++j) {
            const Index v1 = facets(i, j);
            const Index v2 = facets(i, (j + 1) % vertex_per_facet);
            const EdgeType<Index> edge(v1, v2);
            auto it = edge_facet_map.find(edge);
            if (it == edge_facet_map.end()) {
                edge_facet_map[edge] = {i};
            } else {
                it->second.push_back(i);
            }
        }
    }
    return edge_facet_map;
}

// Computes a mapping from each edge to its adjacent facets.
// Only considers the sub-mesh defined by 'active_vertices'
template <typename MeshType>
EdgeFacetMap<MeshType> compute_edge_facet_map_in_active_vertices(
    const MeshType& mesh,
    const std::unordered_set<typename MeshType::Index>& active_vertices)
{
    using Index = typename MeshType::Index;

    std::unordered_set<Index> active_facets;

    if (mesh.is_connectivity_initialized()) {
        // this path is faster but requires connectivity initialized
        for (Index v : active_vertices) {
            for (Index f : mesh.get_facets_adjacent_to_vertex(v)) {
                active_facets.insert(f);
            }
        }
    } else {
        const Index num_facets = mesh.get_num_facets();
        const Index vertex_per_facet = mesh.get_vertex_per_facet();
        const typename MeshType::FacetArray& facets = mesh.get_facets();
        for (Index i = 0; i < num_facets; ++i) {
            if (active_facets.find(i) != active_facets.end()) continue; // already there
            for (Index j = 0; j < vertex_per_facet; ++j) {
                if (active_vertices.find(facets(i, j)) != active_vertices.end()) {
                    active_facets.insert(i);
                    break;
                }
            }
        }
    }
    return compute_edge_facet_map_in_active_facets(mesh, active_facets);
}
} // namespace lagrange

namespace std {
template <typename Index>
struct hash<lagrange::EdgeType<Index>>
{
    std::size_t operator()(const lagrange::EdgeType<Index>& key) const
    {
        return lagrange::safe_cast<std::size_t>(key.v1() + key.v2());
    }
};
} // namespace std
