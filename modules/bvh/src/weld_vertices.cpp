/*
 * Copyright 2025 Adobe. All rights reserved.
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
#include <lagrange/bvh/api.h>
#include <lagrange/bvh/weld_vertices.h>
#include <lagrange/remap_vertices.h>
#include <lagrange/utils/invalid.h>
#include <lagrange/views.h>

#include <Eigen/Core>

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <nanoflann.hpp>
#include <lagrange/utils/warnon.h>
// clang-format on

#include <algorithm>
#include <vector>

namespace lagrange::bvh {

namespace {

template <typename Scalar, typename Index>
void compute_vertex_mapping(
    const SurfaceMesh<Scalar, Index>& mesh,
    Scalar radius,
    std::vector<Index>& vertex_mapping)
{
    const Index num_vertices = mesh.get_num_vertices();
    auto vertices = vertex_view(mesh);
    using VertexArray = decltype(vertices);
    using KDTree = nanoflann::KDTreeEigenMatrixAdaptor<VertexArray>;
    KDTree tree(vertices.cols(), vertices);

    nanoflann::SearchParameters params;
    params.sorted = false;
    std::vector<nanoflann::ResultItem<typename KDTree::IndexType, Scalar>> output;
    output.reserve(32);

    Index vertex_count = 0;
    vertex_mapping.resize(num_vertices, invalid<Index>());
    for (Index i = 0; i < num_vertices; i++) {
        if (vertex_mapping[i] != invalid<Index>()) {
            continue; // Already mapped
        }

        vertex_mapping[i] = vertex_count;
        output.clear();
        tree.index_->radiusSearch(vertices.row(i).data(), radius * radius, output, params);
        for (auto& entry : output) {
            Index j = static_cast<Index>(entry.first);
            if (vertex_mapping[j] == invalid<Index>()) {
                vertex_mapping[j] = vertex_count;
            }
        }
        vertex_count++;
    }
}

template <typename Scalar, typename Index>
void compute_boundary_vertex_mapping(
    SurfaceMesh<Scalar, Index>& mesh,
    Scalar radius,
    std::vector<Index>& vertex_mapping)
{
    mesh.initialize_edges();
    const Index dim = mesh.get_dimension();
    const Index num_vertices = mesh.get_num_vertices();
    const Index num_edges = mesh.get_num_edges();
    auto vertices = vertex_view(mesh);

    std::vector<bool> is_boundary(num_vertices, false);
    for (Index i = 0; i < num_edges; i++) {
        if (mesh.is_boundary_edge(i)) {
            auto [v0, v1] = mesh.get_edge_vertices(i);
            is_boundary[v0] = true;
            is_boundary[v1] = true;
        }
    }

    size_t num_boundary_vertices = std::count(is_boundary.begin(), is_boundary.end(), true);
    std::vector<Scalar> boundary_vertices;
    std::vector<Index> boundary_vertex_indices;
    boundary_vertices.reserve(num_boundary_vertices * dim);
    boundary_vertex_indices.reserve(num_boundary_vertices);

    for (Index i = 0; i < num_vertices; i++) {
        if (is_boundary[i]) {
            std::copy(
                vertices.row(i).data(),
                vertices.row(i).data() + dim,
                std::back_inserter(boundary_vertices));
            boundary_vertex_indices.push_back(i);
        }
    }

    using VertexArray =
        Eigen::Map<Eigen::Matrix<Scalar, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>>;
    using KDTree = nanoflann::KDTreeEigenMatrixAdaptor<VertexArray>;
    VertexArray points(boundary_vertices.data(), boundary_vertices.size() / dim, dim);
    KDTree tree(dim, points);

    nanoflann::SearchParameters params;
    params.sorted = false;
    std::vector<nanoflann::ResultItem<typename KDTree::IndexType, Scalar>> output;
    output.reserve(32);

    Index vertex_count = 0;
    vertex_mapping.resize(num_vertices, invalid<Index>());
    for (Index i = 0; i < num_vertices; i++) {
        if (!is_boundary[i]) {
            // Keep non-boundary vertices as is
            vertex_mapping[i] = vertex_count++;
            continue;
        }

        if (vertex_mapping[i] != invalid<Index>()) {
            continue; // Already mapped
        }

        vertex_mapping[i] = vertex_count;

        output.clear();
        tree.index_->radiusSearch(vertices.row(i).data(), radius * radius, output, params);
        for (auto& entry : output) {
            Index vertex_index = boundary_vertex_indices[static_cast<Index>(entry.first)];
            vertex_mapping[vertex_index] = vertex_count;
        }
        vertex_count++;
    }
}


} // namespace

template <typename Scalar, typename Index>
void weld_vertices(SurfaceMesh<Scalar, Index>& mesh, WeldOptions options)
{
    std::vector<Index> vertex_mapping;
    vertex_mapping.reserve(mesh.get_num_vertices());

    if (options.boundary_only) {
        compute_boundary_vertex_mapping(mesh, static_cast<Scalar>(options.radius), vertex_mapping);
    } else {
        compute_vertex_mapping(mesh, static_cast<Scalar>(options.radius), vertex_mapping);
    }

    RemapVerticesOptions remap_options;
    remap_options.collision_policy_float = options.collision_policy_float;
    remap_options.collision_policy_integral = options.collision_policy_integral;

    remap_vertices(mesh, {vertex_mapping.data(), vertex_mapping.size()}, remap_options);
}


#define LA_X_weld_vertices(_, Scalar, Index) \
    template LA_BVH_API void weld_vertices<Scalar, Index>(SurfaceMesh<Scalar, Index>&, WeldOptions);
LA_SURFACE_MESH_X(weld_vertices, 0)

} // namespace lagrange::bvh
