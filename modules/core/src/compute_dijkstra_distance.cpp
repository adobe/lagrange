/*
 * Copyright 2023 Adobe. All rights reserved.
 * This file is licensed to you under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License. You may obtain a copy
 * of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under
 * the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
 * OF ANY KIND, either express or implied. See the License for the specific language
 * governing permissions and limitations under the License.
 */

#include <lagrange/SurfaceMeshTypes.h>
#include <lagrange/compute_dijkstra_distance.h>
#include <lagrange/internal/dijkstra.h>
#include <lagrange/internal/find_attribute_utils.h>
#include <lagrange/utils/assert.h>
#include <lagrange/views.h>

namespace lagrange {


template <typename Scalar, typename Index>
std::optional<std::vector<Index>> compute_dijkstra_distance(
    SurfaceMesh<Scalar, Index>& mesh,
    const DijkstraDistanceOptions<Scalar, Index>& options)
{
    const auto dist_attr_id = internal::find_or_create_attribute<Scalar>(
        mesh,
        options.output_attribute_name,
        AttributeElement::Vertex,
        AttributeUsage::Scalar,
        1,
        internal::ResetToDefault::No);

    auto dist_data = attribute_vector_ref<Scalar>(mesh, dist_attr_id);
    dist_data.setConstant(Scalar(-1));

    const auto seed_vertices = mesh.get_facet_vertices(options.seed_facet);
    const auto vertices = vertex_view(mesh);

    la_runtime_assert(
        options.barycentric_coords.size() == seed_vertices.size(),
        "Invalid dimension of barycentric coordinates, must match facet size");

    // Compute given point using barycentric coordinates
    Vector<Scalar> pt = Vector<Scalar>::Zero(vertices.cols());
    for (Index i = 0; i < seed_vertices.size(); i++) {
        pt += vertices.row(seed_vertices[i]) * options.barycentric_coords[i];
    }
    // Compute initial distance to that point
    Vector<Scalar> initial_dist = Vector<Scalar>::Zero(seed_vertices.size());
    for (Index i = 0; i < seed_vertices.size(); i++) {
        initial_dist[i] = (vertices.row(seed_vertices[i]).transpose() - pt).norm();
    }

    const auto dist = [&](Index vi, Index vj) {
        return (vertices.row(vi) - vertices.row(vj)).norm();
    };

    std::optional<std::vector<Index>> involved_vts;
    if (options.output_involved_vertices) {
        involved_vts = std::vector<Index>();
    }

    std::function<bool(Index, Scalar)> process;
    if (options.output_involved_vertices) {
        process = [&](Index vi, Scalar d) {
            dist_data[vi] = d;
            involved_vts->push_back(vi);
            return false;
        };
    } else {
        process = [&](Index vi, Scalar d) {
            dist_data[vi] = d;
            return false;
        };
    }

    internal::dijkstra<Scalar, Index>(
        mesh,
        seed_vertices,
        span<const Scalar>(initial_dist.data(), static_cast<size_t>(initial_dist.size())),
        options.radius,
        dist,
        process);

    return involved_vts;
}

#define LA_X_compute_dijkstra_distance(_, Scalar, Index)                                 \
    template LA_CORE_API std::optional<std::vector<Index>> compute_dijkstra_distance<Scalar, Index>( \
        SurfaceMesh<Scalar, Index>&,                                                     \
        const DijkstraDistanceOptions<Scalar, Index>& options);
LA_SURFACE_MESH_X(compute_dijkstra_distance, 0)

} // namespace lagrange
