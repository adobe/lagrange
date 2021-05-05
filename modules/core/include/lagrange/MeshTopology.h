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

#include <tbb/blocked_range.h>
#include <tbb/enumerable_thread_specific.h>
#include <tbb/parallel_reduce.h>

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
        mesh.initialize_edge_data_new();
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
        m_euler = (int)mesh.get_num_vertices() + (int)mesh.get_num_facets() -
                  (int)mesh.get_num_edges_new();
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
        const Index num_edges = mesh.get_num_edges_new();
        for (auto ei : range(num_edges)) {
            if (mesh.get_num_facets_around_edge_new(ei) > 2) return false;
        }
        return true;
    }

    bool check_vertex_manifold(const MeshType& mesh) const
    {
        LA_ASSERT(
            mesh.get_vertex_per_facet() == 3,
            "Vertex manifold check only supports triangle mesh for now.");

        const auto num_vertices = mesh.get_num_vertices();
        const auto& facets = mesh.get_facets();
        tbb::enumerable_thread_specific<std::vector<Edge>> thread_rim_edges;

        auto is_vertex_manifold = [&](Index vid) {
            auto& rim_edges = thread_rim_edges.local();
            rim_edges.clear();
            rim_edges.reserve(mesh.get_num_facets_around_vertex_new(vid));

            mesh.foreach_corners_around_vertex_new(vid, [&](Index ci) {
                Index fid = ci / 3;
                Index lv = ci % 3;
                rim_edges.push_back({facets(fid, (lv + 1) % 3), facets(fid, (lv + 2) % 3)});
            });

            const auto chains = chain_edges<Index>(rim_edges);
            return (chains.size() == 1);
        };

        return tbb::parallel_reduce(
            tbb::blocked_range<Index>(0, num_vertices),
            true, ///< initial value of the result.
            [&](const tbb::blocked_range<Index>& r, bool manifold) -> bool {
                if (!manifold) return false;

                for (Index vi = r.begin(); vi != r.end(); vi++) {
                    if (!is_vertex_manifold(vi)) {
                        return false;
                    }
                }
                return true;
            },
            [](bool r1, bool r2) -> bool { return r1 && r2; });
    }

private:
    std::vector<std::vector<typename MeshType::Index>> m_boundary_loops;
    int m_euler;
    bool m_is_vertex_manifold;
    bool m_is_edge_manifold;
    bool m_initialized;
};

} // namespace lagrange
