/*
 * Copyright 2026 Adobe. All rights reserved.
 * This file is licensed to you under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License. You may obtain a copy
 * of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under
 * the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
 * OF ANY KIND, either express or implied. See the License for the specific language
 * governing permissions and limitations under the License.
 */
#include <lagrange/bvh/compute_mesh_distances.h>

#include <lagrange/Attribute.h>
#include <lagrange/AttributeTypes.h>
#include <lagrange/SurfaceMeshTypes.h>
#include <lagrange/bvh/TriangleAABBTree.h>
#include <lagrange/internal/find_attribute_utils.h>
#include <lagrange/utils/assert.h>
#include <lagrange/views.h>

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <tbb/parallel_for.h>
#include <tbb/parallel_reduce.h>
#include <lagrange/utils/warnon.h>
// clang-format on

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

namespace lagrange::bvh {

namespace {

///
/// Compute the distance from each vertex in @p mesh to the closest point on @p tree,
/// writing the results into the pre-allocated output span @p out_distances.
///
/// @pre out_distances.size() == mesh.get_num_vertices()
///
template <typename Scalar, typename Index>
void compute_vertex_distances(
    const SurfaceMesh<Scalar, Index>& mesh,
    const TriangleAABBTree<Scalar, Index>& tree,
    span<Scalar> out_distances)
{
    const Index num_vertices = mesh.get_num_vertices();
    la_debug_assert(out_distances.size() == num_vertices);

    if (tree.empty()) {
        std::fill(out_distances.begin(), out_distances.end(), Scalar(0));
        return;
    }

    auto vertices = vertex_view(mesh);
    using RowVector = typename TriangleAABBTree<Scalar, Index>::RowVectorType;

    tbb::parallel_for(Index(0), num_vertices, [&](Index vi) {
        RowVector closest_pt;
        Index tri_id;
        Scalar sq_dist;
        tree.get_closest_point(vertices.row(vi), tri_id, closest_pt, sq_dist);
        out_distances[vi] = std::sqrt(sq_dist);
    });
}

} // namespace

template <typename Scalar, typename Index>
AttributeId compute_mesh_distances(
    SurfaceMesh<Scalar, Index>& source,
    const SurfaceMesh<Scalar, Index>& target,
    const MeshDistancesOptions& options)
{
    la_runtime_assert(
        source.get_dimension() == target.get_dimension(),
        "Source and target meshes must have the same spatial dimension.");
    la_runtime_assert(target.is_triangle_mesh(), "Target mesh must be a triangle mesh.");

    const AttributeId attr_id = internal::find_or_create_attribute<Scalar>(
        source,
        options.output_attribute_name,
        AttributeElement::Vertex,
        AttributeUsage::Scalar,
        1,
        internal::ResetToDefault::No);

    TriangleAABBTree<Scalar, Index> tree(target);

    // Write directly into the attribute buffer — no temporary vector needed.
    auto& attr = source.template ref_attribute<Scalar>(attr_id);
    compute_vertex_distances(source, tree, attr.ref_all());

    return attr_id;
}

template <typename Scalar, typename Index>
Scalar compute_hausdorff(
    const SurfaceMesh<Scalar, Index>& source,
    const SurfaceMesh<Scalar, Index>& target)
{
    la_runtime_assert(
        source.get_dimension() == target.get_dimension(),
        "Source and target meshes must have the same spatial dimension.");
    la_runtime_assert(source.is_triangle_mesh(), "Source mesh must be a triangle mesh.");
    la_runtime_assert(target.is_triangle_mesh(), "Target mesh must be a triangle mesh.");

    // Directed source → target.
    TriangleAABBTree<Scalar, Index> tree_target(target);
    std::vector<Scalar> dist_fwd(source.get_num_vertices());
    compute_vertex_distances(source, tree_target, span<Scalar>(dist_fwd));
    Scalar d_fwd =
        dist_fwd.empty() ? Scalar(0) : *std::max_element(dist_fwd.begin(), dist_fwd.end());

    // Directed target → source.
    TriangleAABBTree<Scalar, Index> tree_source(source);
    std::vector<Scalar> dist_bwd(target.get_num_vertices());
    compute_vertex_distances(target, tree_source, span<Scalar>(dist_bwd));
    Scalar d_bwd =
        dist_bwd.empty() ? Scalar(0) : *std::max_element(dist_bwd.begin(), dist_bwd.end());

    return std::max(d_fwd, d_bwd);
}

template <typename Scalar, typename Index>
Scalar compute_chamfer(
    const SurfaceMesh<Scalar, Index>& source,
    const SurfaceMesh<Scalar, Index>& target)
{
    la_runtime_assert(
        source.get_dimension() == target.get_dimension(),
        "Source and target meshes must have the same spatial dimension.");
    la_runtime_assert(source.is_triangle_mesh(), "Source mesh must be a triangle mesh.");
    la_runtime_assert(target.is_triangle_mesh(), "Target mesh must be a triangle mesh.");

    // Source → target distances.
    TriangleAABBTree<Scalar, Index> tree_target(target);
    std::vector<Scalar> dist_fwd(source.get_num_vertices());
    compute_vertex_distances(source, tree_target, span<Scalar>(dist_fwd));

    // Target → source distances.
    TriangleAABBTree<Scalar, Index> tree_source(source);
    std::vector<Scalar> dist_bwd(target.get_num_vertices());
    compute_vertex_distances(target, tree_source, span<Scalar>(dist_bwd));

    auto sum_squared = [](const std::vector<Scalar>& d) -> Scalar {
        return tbb::parallel_reduce(
            tbb::blocked_range<size_t>(0, d.size()),
            Scalar(0),
            [&](const tbb::blocked_range<size_t>& r, Scalar acc) {
                for (size_t i = r.begin(); i < r.end(); ++i) acc += d[i] * d[i];
                return acc;
            },
            std::plus<Scalar>{});
    };

    Scalar chamfer = Scalar(0);
    if (!dist_fwd.empty()) chamfer += sum_squared(dist_fwd) / static_cast<Scalar>(dist_fwd.size());
    if (!dist_bwd.empty()) chamfer += sum_squared(dist_bwd) / static_cast<Scalar>(dist_bwd.size());

    return chamfer;
}

// Explicit instantiations.
#define LA_X_compute_mesh_distances(_, Scalar, Index)                      \
    template LA_BVH_API AttributeId compute_mesh_distances<Scalar, Index>( \
        SurfaceMesh<Scalar, Index>&,                                       \
        const SurfaceMesh<Scalar, Index>&,                                 \
        const MeshDistancesOptions&);                                      \
    template LA_BVH_API Scalar compute_hausdorff<Scalar, Index>(           \
        const SurfaceMesh<Scalar, Index>&,                                 \
        const SurfaceMesh<Scalar, Index>&);                                \
    template LA_BVH_API Scalar compute_chamfer<Scalar, Index>(             \
        const SurfaceMesh<Scalar, Index>&,                                 \
        const SurfaceMesh<Scalar, Index>&);
LA_SURFACE_MESH_X(compute_mesh_distances, 0)

} // namespace lagrange::bvh
