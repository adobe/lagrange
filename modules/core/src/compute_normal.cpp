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
#include <lagrange/DisjointSets.h>
#include <lagrange/IndexedAttribute.h>
#include <lagrange/Logger.h>
#include <lagrange/SurfaceMeshTypes.h>
#include <lagrange/compute_facet_normal.h>
#include <lagrange/compute_normal.h>
#include <lagrange/utils/function_ref.h>
#include <lagrange/utils/geometry3d.h>
#include <lagrange/views.h>

#include "internal/compute_weighted_corner_normal.h"
#include "internal/retrieve_normal_attribute.h"

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
            if (mesh.count_num_corners_around_edge(ei) != 1) {
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
            if (mesh.count_num_corners_around_edge(ei) != 1) {
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

template <typename Scalar, typename Index>
auto recompute_facet_normal_if_needed(
    SurfaceMesh<Scalar, Index>& mesh,
    const NormalOptions& options)
{
    AttributeId facet_normal_id;
    bool had_facet_normals = mesh.has_attribute(options.facet_normal_attribute_name);
    if (options.recompute_facet_normals || !had_facet_normals) {
        FacetNormalOptions facet_normal_options;
        facet_normal_options.output_attribute_name = options.facet_normal_attribute_name;
        facet_normal_id = compute_facet_normal(mesh, facet_normal_options);
    } else {
        facet_normal_id =
            internal::retrieve_normal_attribute(mesh, options.facet_normal_attribute_name, Facet);
    }
    return std::make_pair(
        matrix_view(mesh.template get_attribute<Scalar>(facet_normal_id)),
        had_facet_normals);
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

    auto [facet_normal, had_facet_normals] = recompute_facet_normal_if_needed(mesh, options);

    const auto num_vertices = mesh.get_num_vertices();
    const auto num_corners = mesh.get_num_corners();

    std::vector<bool> is_cone_vertex(num_vertices, false);
    for (auto vi : cone_vertices) is_cone_vertex[vi] = true;

    // Step 1: For each vertex v, iterate over its incident corners and
    // group corners sharing the same normal together.
    DisjointSets<Index> unified_indices = get_unified_indices(is_cone_vertex);

    // Step 2: Gather corner groups.
    AttributeId attr_id =
        internal::retrieve_normal_attribute(mesh, options.output_attribute_name, Indexed);
    auto& attr = mesh.template ref_indexed_attribute<Scalar>(attr_id);
    auto attr_indices = attr.indices().ref_all();
    la_debug_assert(attr_indices.size() == num_corners);
    std::fill(attr_indices.begin(), attr_indices.end(), invalid<Index>());

    Index num_indices = 0;
    for (Index n = 0; n < num_corners; ++n) {
        Index r = unified_indices.find(n);
        if (attr_indices[r] == invalid<Index>()) {
            attr_indices[r] = num_indices++;
        }
        attr_indices[n] = attr_indices[r];
    }

    std::vector<Index> indices(num_corners);
    std::vector<Index> offsets(num_indices + 1, 0);
    for (Index c = 0; c < num_corners; ++c) {
        offsets[attr_indices[c] + 1]++;
    }
    for (Index r = 1; r <= num_indices; ++r) {
        offsets[r] += offsets[r - 1];
    }
    {
        // Bucket sort for corner indices
        std::vector<Index> counter = offsets;
        for (Index c = 0; c < num_corners; c++) {
            indices[counter[attr_indices[c]]++] = c;
        }
    }

    // Step 3: Compute and average corner normals.
    auto& attr_values = attr.values();
    attr_values.set_default_value(0);
    attr_values.resize_elements(num_indices);
    auto normal_values = matrix_ref(attr_values);
    normal_values.setZero();

    auto compute_weighted_corner_normal =
        [&, facet_normal = facet_normal](Index ci) -> Eigen::Matrix<Scalar, 3, 1> {
        auto n = internal::compute_weighted_corner_normal(mesh, ci, options.weight_type);
        Scalar sign = std::copysign(1, n.dot(facet_normal.row(mesh.get_corner_facet(ci))));
        n *= sign;
        return n;
    };

    tbb::parallel_for((Index)0, num_indices, [&](Index r) {
        for (Index i = offsets[r]; i < offsets[r + 1]; i++) {
            Index c = indices[i];
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

    auto [facet_normal, had_facet_normals] = recompute_facet_normal_if_needed(mesh, options);

    auto is_smooth = [&, facet_normal = facet_normal](Index fi, Index fj) -> bool {
        const Eigen::Matrix<Scalar, 1, 3> ni = facet_normal.row(fi);
        const Eigen::Matrix<Scalar, 1, 3> nj = facet_normal.row(fj);

        const auto theta = angle_between(ni, nj);
        return theta < feature_angle_threshold;
    };

    auto attr_id = compute_normal<Scalar, Index>(mesh, is_smooth, cone_vertices, options);
    if (!options.keep_facet_normals && !had_facet_normals) {
        mesh.delete_attribute(options.facet_normal_attribute_name);
    }
    return attr_id;
}

#define LA_X_compute_normal(_, Scalar, Index)           \
    template AttributeId compute_normal<Scalar, Index>( \
        SurfaceMesh<Scalar, Index> & mesh,              \
        function_ref<bool(Index)>,                      \
        span<const Index>,                              \
        NormalOptions);                                 \
    template AttributeId compute_normal<Scalar, Index>( \
        SurfaceMesh<Scalar, Index> & mesh,              \
        function_ref<bool(Index, Index)>,               \
        span<const Index>,                              \
        NormalOptions);                                 \
    template AttributeId compute_normal<Scalar, Index>( \
        SurfaceMesh<Scalar, Index> & mesh,              \
        Scalar,                                         \
        span<const Index>,                              \
        NormalOptions);
LA_SURFACE_MESH_X(compute_normal, 0)

} // namespace lagrange
