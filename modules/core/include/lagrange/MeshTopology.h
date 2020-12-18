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

#include <memory>
#include <stdexcept>
#include <vector>

#include <lagrange/chain_edges.h>
#include <lagrange/extract_boundary_loops.h>
#include <lagrange/get_opposite_edge.h>
#include <lagrange/utils/safe_cast.h>

namespace lagrange {

template <typename MeshType>
class MeshTopology
{
public:
    using IndexList = typename MeshType::IndexList;
    using Edge = typename MeshType::Edge;
    using Index = typename MeshType::Index;

public:
    MeshTopology()
        : m_euler(0)
        , m_is_vertex_manifold(false)
        , m_is_edge_manifold(false)
        , m_initialized(false)
    {}

    void initialize(MeshType& mesh)
    {
        if (!mesh.is_connectivity_initialized()) {
            mesh.initialize_connectivity();
        }
        if (!mesh.is_edge_data_initialized()) {
            mesh.initialize_edge_data();
        }

        initialize_euler(mesh);
        initialize_manifoldness(mesh);
        initialize_boundary(mesh);

        m_initialized = true;
    }

    bool is_initialized() const { return m_initialized; }

    const std::vector<std::vector<Index>>& get_boundaries() const { return m_boundary_loops; }

    int get_euler() const { return m_euler; }

    bool is_cylinder() const
    {
        return is_manifold() && m_euler == 0 && m_boundary_loops.size() == 2;
    }

    bool is_disc() const { return is_manifold() && m_euler == 1 && m_boundary_loops.size() == 1; }

    bool is_manifold() const { return m_is_vertex_manifold && m_is_edge_manifold; }

    bool is_vertex_manifold() const { return m_is_vertex_manifold; }

    bool is_edge_manifold() const { return m_is_edge_manifold; }

    const std::vector<std::vector<Index>>& get_boundary_loops() const { return m_boundary_loops; }

protected:
    void initialize_euler(const MeshType& mesh)
    {
        m_euler =
            (int)mesh.get_num_vertices() + (int)mesh.get_num_facets() - (int)mesh.get_num_edges();
    }

    void initialize_manifoldness(const MeshType& mesh)
    {
        m_is_edge_manifold = check_edge_manifold(mesh);
        if (m_is_edge_manifold) {
            m_is_vertex_manifold = check_vertex_manifold(mesh);
        } else {
            m_is_vertex_manifold = false;
        }
    }

    void initialize_boundary(MeshType& mesh)
    {
        if (is_manifold()) {
            m_boundary_loops = extract_boundary_loops(mesh);
        } else {
            // Only simple boundary loop is supported.
            m_boundary_loops.clear();
        }
    }

    bool check_edge_manifold(const MeshType& mesh) const
    {
        for (const auto& adj_facets : mesh.get_edge_facet_adjacency()) {
            if (adj_facets.size() > 2) return false;
        }
        return true;
    }

    bool check_vertex_manifold(const MeshType& mesh) const
    {
        LA_ASSERT(mesh.is_connectivity_initialized(), "Connectivity is not initialized");
        LA_ASSERT(
            mesh.get_vertex_per_facet() == 3,
            "Vertex manifold check only supports triangle mesh for now.");

        const auto num_vertices = mesh.get_num_vertices();
        const auto& facets = mesh.get_facets();

        std::vector<Edge> rim_edges;
        for (Index i = 0; i < num_vertices; i++) {
            const auto& adj_facets = mesh.get_facets_adjacent_to_vertex(i);

            rim_edges.clear();
            rim_edges.reserve(adj_facets.size());
            for (const auto fid : adj_facets) {
                rim_edges.push_back(get_opposite_edge(facets, fid, i));
            }

            const auto chains = chain_edges<Index>(rim_edges);
            if (chains.size() > 1) return false;
        }
        return true;
    }

private:
    std::vector<std::vector<typename MeshType::Index>> m_boundary_loops;
    int m_euler;
    bool m_is_vertex_manifold;
    bool m_is_edge_manifold;
    bool m_initialized;
};

} // namespace lagrange
