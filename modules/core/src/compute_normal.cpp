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

#include <lagrange/Attribute.h>
#include <lagrange/AttributeFwd.h>
#include <lagrange/IndexedAttribute.h>
#include <lagrange/Logger.h>
#include <lagrange/SurfaceMeshTypes.h>
#include <lagrange/compute_facet_normal.h>
#include <lagrange/compute_normal.h>
#include <lagrange/internal/find_attribute_utils.h>
#include <lagrange/utils/function_ref.h>
#include <lagrange/utils/geometry3d.h>
#include <lagrange/views.h>

#include "internal/bucket_sort.h"
#include "internal/compute_weighted_corner_normal.h"
#include "internal/recompute_facet_normal_if_needed.h"

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <tbb/parallel_for.h>
#include <lagrange/utils/warnon.h>
// clang-format on

#include <Eigen/Core>

#include <array>
#include <string>
#include <vector>

namespace lagrange {

////////////////////////////////////////////////////////////////////////////////

namespace {

template <typename Scalar, typename Index>
DisjointSets<Index> compute_unified_indices(
    const SurfaceMesh<Scalar, Index>& mesh,
    function_ref<bool(Index)> is_edge_smooth,
    const std::vector<bool>& is_cone_vertex)
{
    DisjointSets<Index> unified_indices(mesh.get_num_corners());
    tbb::parallel_for(Index(0), mesh.get_num_vertices(), [&](Index vi) {
        if (is_cone_vertex[vi]) return;
        mesh.foreach_corner_around_vertex(vi, [&](Index ci) {
            const Index ei = mesh.get_corner_edge(ci);
            const Index fi = mesh.get_corner_facet(ci);
            if (mesh.count_num_corners_around_edge(ei) != 2) {
                return; // Boundary or non-manifold, don't merge corners
            }
            if (!is_edge_smooth(ei)) {
                return;
            }
            mesh.foreach_corner_around_edge(ei, [&](Index cj) {
                Index vj = mesh.get_corner_vertex(cj);
                const Index fj = mesh.get_corner_facet(cj);
                if (fi == fj) return;
                if (vi != vj) {
                    cj = (cj + 1 == mesh.get_facet_corner_end(fj)) ? mesh.get_facet_corner_begin(fj)
                                                                   : cj + 1;
                    vj = mesh.get_corner_vertex(cj);
                    la_debug_assert(vi == vj);
                }
                unified_indices.merge(ci, cj);
            });
        });
    });

    return unified_indices;
}

template <typename Scalar, typename Index>
DisjointSets<Index> compute_unified_indices(
    const SurfaceMesh<Scalar, Index>& mesh,
    function_ref<bool(Index, Index)> is_edge_smooth,
    const std::vector<bool>& is_cone_vertex)
{
    DisjointSets<Index> unified_indices(mesh.get_num_corners());
    tbb::parallel_for(Index(0), mesh.get_num_vertices(), [&](Index vi) {
        if (is_cone_vertex[vi]) return;
        mesh.foreach_corner_around_vertex(vi, [&](Index ci) {
            const Index ei = mesh.get_corner_edge(ci);
            const Index fi = mesh.get_corner_facet(ci);
            if (mesh.count_num_corners_around_edge(ei) != 2) {
                return; // Boundary or non-manifold, don't merge corners
            }
            mesh.foreach_corner_around_edge(ei, [&](Index cj) {
                Index vj = mesh.get_corner_vertex(cj);
                const Index fj = mesh.get_corner_facet(cj);
                if (fi == fj) return;
                if (vi != vj) {
                    cj = (cj + 1 == mesh.get_facet_corner_end(fj)) ? mesh.get_facet_corner_begin(fj)
                                                                   : cj + 1;
                    vj = mesh.get_corner_vertex(cj);
                    la_debug_assert(vi == vj);
                }
                if (is_edge_smooth(fi, fj)) {
                    unified_indices.merge(ci, cj);
                }
            });
        });
    });

    return unified_indices;
}

template <typename Scalar, typename Index, typename Func>
AttributeId compute_normal_internal(
    SurfaceMesh<Scalar, Index>& mesh,
    span<const Index> cone_vertices,
    const NormalOptions& options,
    Func get_unified_indices)
{
    la_runtime_assert(mesh.get_dimension() == 3, "Only 3D meshes are supported.");
    if (!mesh.has_edges()) mesh.initialize_edges();

    auto [facet_normal_id, had_facet_normals] = internal::recompute_facet_normal_if_needed(
        mesh,
        options.facet_normal_attribute_name,
        options.recompute_facet_normals);
    auto facet_normal = attribute_matrix_view<Scalar>(mesh, facet_normal_id);

    const auto num_vertices = mesh.get_num_vertices();

    std::vector<bool> is_cone_vertex(num_vertices, false);
    for (auto vi : cone_vertices) is_cone_vertex[vi] = true;

    // Step 1: For each vertex v, iterate over its incident corners and
    // group corners sharing the same normal together.
    DisjointSets<Index> unified_indices = get_unified_indices(is_cone_vertex);

    // Step 2: Sort corner indices according to groups.
    AttributeId attr_id = internal::find_or_create_attribute<Scalar>(
        mesh,
        options.output_attribute_name,
        Indexed,
        AttributeUsage::Normal,
        3,
        internal::ResetToDefault::Yes);

    auto& attr = mesh.template ref_indexed_attribute<Scalar>(attr_id);
    auto attr_indices = attr.indices().ref_all();
    la_debug_assert(attr_indices.size() == mesh.get_num_corners());

    auto buckets = internal::bucket_sort(unified_indices, attr_indices);

    // Step 3: Compute and average corner normals.
    auto& attr_values = attr.values();
    attr_values.resize_elements(buckets.num_representatives);
    auto normal_values = matrix_ref(attr_values);

    auto compute_weighted_corner_normal =
        [&, facet_normal = facet_normal](Index ci) -> Eigen::Matrix<Scalar, 3, 1> {
        auto n = internal::compute_weighted_corner_normal(mesh, ci, options.weight_type);
        Scalar sign = std::copysign(1.f, n.dot(facet_normal.row(mesh.get_corner_facet(ci))));
        n *= sign;
        return n;
    };

    tbb::parallel_for((Index)0, buckets.num_representatives, [&](Index r) {
        for (Index i = buckets.representative_offsets[r]; i < buckets.representative_offsets[r + 1];
             ++i) {
            Index c = buckets.sorted_elements[i];
            normal_values.row(r) += compute_weighted_corner_normal(c).transpose();
        }
        normal_values.row(r).stableNormalize();
    });

    if (!options.keep_facet_normals && !had_facet_normals) {
        mesh.delete_attribute(options.facet_normal_attribute_name);
    }

    return attr_id;
}

} // namespace

////////////////////////////////////////////////////////////////////////////////

template <typename Scalar, typename Index>
AttributeId compute_normal(
    SurfaceMesh<Scalar, Index>& mesh,
    function_ref<bool(Index)> is_edge_smooth,
    span<const Index> cone_vertices,
    NormalOptions options)
{
    return compute_normal_internal(mesh, cone_vertices, options, [&](auto&& is_cone_vertex) {
        return compute_unified_indices(mesh, is_edge_smooth, is_cone_vertex);
    });
}

template <typename Scalar, typename Index>
AttributeId compute_normal(
    SurfaceMesh<Scalar, Index>& mesh,
    function_ref<bool(Index, Index)> is_edge_smooth,
    span<const Index> cone_vertices,
    NormalOptions options)
{
    return compute_normal_internal(mesh, cone_vertices, options, [&](auto&& is_cone_vertex) {
        return compute_unified_indices(mesh, is_edge_smooth, is_cone_vertex);
    });
}

template <typename Scalar, typename Index>
AttributeId compute_normal(
    SurfaceMesh<Scalar, Index>& mesh,
    Scalar feature_angle_threshold,
    span<const Index> cone_vertices,
    NormalOptions options)
{
    la_runtime_assert(mesh.get_dimension() == 3, "Only 3D meshes are supported.");

    if (!mesh.has_edges()) mesh.initialize_edges();

    auto [facet_normal_id, had_facet_normals] = internal::recompute_facet_normal_if_needed(
        mesh,
        options.facet_normal_attribute_name,
        options.recompute_facet_normals);
    auto facet_normal = attribute_matrix_view<Scalar>(mesh, facet_normal_id);

    auto is_smooth = [&, facet_normal = facet_normal](Index fi, Index fj) -> bool {
        const Eigen::Matrix<Scalar, 1, 3> ni = facet_normal.row(fi);
        const Eigen::Matrix<Scalar, 1, 3> nj = facet_normal.row(fj);

        const auto theta = angle_between(ni, nj);
        return theta < feature_angle_threshold;
    };

    // No need to recompute facet normals again as they are already recomputed.
    options.recompute_facet_normals = false;

    auto attr_id = compute_normal<Scalar, Index>(mesh, is_smooth, cone_vertices, options);
    if (!options.keep_facet_normals && !had_facet_normals) {
        mesh.delete_attribute(options.facet_normal_attribute_name);
    }
    return attr_id;
}

#define LA_X_compute_normal(_, Scalar, Index)           \
    template LA_CORE_API AttributeId compute_normal<Scalar, Index>( \
        SurfaceMesh<Scalar, Index> & mesh,              \
        function_ref<bool(Index)>,                      \
        span<const Index>,                              \
        NormalOptions);                                 \
    template LA_CORE_API AttributeId compute_normal<Scalar, Index>( \
        SurfaceMesh<Scalar, Index> & mesh,              \
        function_ref<bool(Index, Index)>,               \
        span<const Index>,                              \
        NormalOptions);                                 \
    template LA_CORE_API AttributeId compute_normal<Scalar, Index>( \
        SurfaceMesh<Scalar, Index> & mesh,              \
        Scalar,                                         \
        span<const Index>,                              \
        NormalOptions);
LA_SURFACE_MESH_X(compute_normal, 0)

} // namespace lagrange
