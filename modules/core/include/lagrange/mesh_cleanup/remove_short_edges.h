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
#include <limits>
#include <memory>

#include <lagrange/DisjointSets.h>
#include <lagrange/Mesh.h>
#include <lagrange/MeshTrait.h>
#include <lagrange/attributes/map_attributes.h>
#include <lagrange/common.h>
#include <lagrange/compute_edge_lengths.h>
#include <lagrange/create_mesh.h>
#include <lagrange/mesh_cleanup/remove_isolated_vertices.h>
#include <lagrange/mesh_cleanup/remove_topologically_degenerate_triangles.h>
#include <lagrange/utils/safe_cast.h>

namespace lagrange {

/**
 * Remove edges shorter than a given tolerance.
 *
 * Arguments:
 *     mesh: input mesh.
 *     tol: edges with length <= tol will be removed.
 *
 * Returns:
 *     An output mesh without edge <= tol.
 */
template <typename MeshType>
std::unique_ptr<MeshType> remove_short_edges(
    const MeshType& in_mesh,
    typename MeshType::Scalar tol = 0.0)
{
    static_assert(MeshTrait<MeshType>::is_mesh(), "Input type is not Mesh");
    using Index = typename MeshType::Index;

    lagrange::logger().trace("[remove_short_edges]");

    // Topological degeneracy can affect the index mapping algorithm used here.
    // So eliminating topological degeneracy first.
    auto meshPtr = remove_topologically_degenerate_triangles(in_mesh);
    auto& mesh = *meshPtr;

    const auto num_vertices = mesh.get_num_vertices();
    const auto num_facets = mesh.get_num_facets();
    const auto vertex_per_facet = mesh.get_vertex_per_facet();

    DisjointSets<Index> clusters(num_vertices);

    compute_edge_lengths(mesh);
    const auto num_edges = mesh.get_num_edges_new();
    const auto& edge_lengths = mesh.get_edge_attribute_new("length");
    for (Index edge_idx = 0; edge_idx < num_edges; ++edge_idx) {
        const auto& edge = mesh.get_edge_vertices_new(edge_idx);
        const auto& length = edge_lengths(edge_idx);
        if (length <= tol) {
            clusters.merge(edge[0], edge[1]);
        }
    }

    auto facets = mesh.get_facets(); // Intentionally copying the data.
    std::transform(
        facets.data(),
        facets.data() + num_facets * vertex_per_facet,
        facets.data(),
        [&](Index i) { return clusters.find(i); });
    auto mesh2 = create_mesh(mesh.get_vertices(), std::move(facets));

    // Above we create topologically degenerate triangles, but vertex and
    // facet indexes don't actually change, so we map attributes directly
    map_attributes(mesh, *mesh2);

    mesh2 = remove_topologically_degenerate_triangles(*mesh2);
    mesh2 = remove_isolated_vertices(*mesh2);
    return mesh2;
}

} // namespace lagrange
