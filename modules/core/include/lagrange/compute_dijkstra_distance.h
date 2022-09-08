/*
 * Copyright 2017 Adobe. All rights reserved.
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

#include <cassert>
#include <limits>
#include <list>
#include <queue>
#include <vector>

#include <lagrange/Mesh.h>
#include <lagrange/common.h>
#include <lagrange/compute_triangle_normal.h>

namespace lagrange {

template <typename MeshType>
std::list<std::pair<typename MeshType::Index, typename MeshType::Scalar>> compute_dijkstra_distance(
    MeshType& mesh,
    typename MeshType::Index seed_facet_id,
    const Eigen::Matrix<typename MeshType::Scalar, 3, 1>& bc,
    typename MeshType::Scalar radius = 0.0)
{
    using Scalar = typename MeshType::Scalar;
    if (mesh.get_dim() != 3) {
        throw std::runtime_error("Input mesh must be 3D mesh.");
    }
    if (mesh.get_vertex_per_facet() != 3) {
        throw std::runtime_error("Input mesh is not triangle mesh");
    }
    if (!mesh.has_facet_attribute("normal")) {
        compute_triangle_normal(mesh);
    }
    if (!mesh.is_connectivity_initialized()) {
        mesh.initialize_connectivity();
    }

    using Index = typename MeshType::Index;
    using FacetType = Eigen::Matrix<Index, 3, 1>;
    using VertexType = typename MeshType::VertexType;
    using AttributeArray = typename MeshType::AttributeArray;

    if (radius <= 0.0) {
        radius = std::numeric_limits<Scalar>::max();
    }
    const Index num_facets = mesh.get_num_facets();
    const Index num_vertices = mesh.get_num_vertices();
    const auto& vertices = mesh.get_vertices();
    const auto& facets = mesh.get_facets();
    la_runtime_assert(seed_facet_id < num_facets);
    const FacetType seed_facet = facets.row(seed_facet_id);
    const VertexType seed_point = vertices.row(seed_facet[0]) * bc[0] +
                                  vertices.row(seed_facet[1]) * bc[1] +
                                  vertices.row(seed_facet[2]) * bc[2];

    using Entry = std::pair<Index, Scalar>;
    auto comp = [](const Entry& e1, const Entry& e2) { return e1.second > e2.second; };
    std::priority_queue<Entry, std::vector<Entry>, decltype(comp)> Q(comp);
    AttributeArray dist(num_vertices, 1);
    dist.setConstant(-1);
    Q.push({seed_facet[0], (vertices.row(seed_facet[0]) - seed_point).norm()});
    Q.push({seed_facet[1], (vertices.row(seed_facet[1]) - seed_point).norm()});
    Q.push({seed_facet[2], (vertices.row(seed_facet[2]) - seed_point).norm()});

    std::list<Entry> involved_vts;
    while (!Q.empty()) {
        Entry entry = Q.top();
        involved_vts.push_back(entry);
        Q.pop();
        Scalar curr_dist = dist(entry.first, 0);

        if (curr_dist < 0 || entry.second < curr_dist) {
            dist(entry.first, 0) = entry.second;
            const auto& adj_vertices = mesh.get_vertices_adjacent_to_vertex(entry.first);
            for (const auto& vj : adj_vertices) {
                Scalar d = entry.second + (vertices.row(vj) - vertices.row(entry.first)).norm();
                if (d < radius) {
                    Scalar next_dist = dist(vj, 0);
                    if (next_dist < 0 || d < next_dist) {
                        Q.push({vj, d});
                    }
                }
            }
        }
    }

    mesh.add_vertex_attribute("dijkstra_distance");
    mesh.import_vertex_attribute("dijkstra_distance", dist);
    return involved_vts;
}
} // namespace lagrange
