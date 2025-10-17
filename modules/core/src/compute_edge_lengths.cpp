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

#include <lagrange/AttributeFwd.h>
#include <lagrange/SurfaceMeshTypes.h>
#include <lagrange/compute_edge_lengths.h>
#include <lagrange/internal/find_attribute_utils.h>
#include <lagrange/views.h>

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <tbb/parallel_for.h>
#include <lagrange/utils/warnon.h>
// clang-format on

namespace lagrange {

template <typename Scalar, typename Index>
AttributeId compute_edge_lengths(SurfaceMesh<Scalar, Index>& mesh, const EdgeLengthOptions& options)
{
    mesh.initialize_edges();

    AttributeId attr_id = internal::find_or_create_attribute<Scalar>(
        mesh,
        options.output_attribute_name,
        Edge,
        AttributeUsage::Scalar,
        1,
        internal::ResetToDefault::No);
    auto edge_lengths = attribute_matrix_ref<Scalar>(mesh, attr_id);

    auto vertices = vertex_view(mesh);
    const auto num_edges = mesh.get_num_edges();
    tbb::parallel_for((Index)0, num_edges, [&](Index ei) {
        auto end_points = mesh.get_edge_vertices(ei);
        edge_lengths(ei) = (vertices.row(end_points[0]) - vertices.row(end_points[1])).norm();
    });

    return attr_id;
}

#define LA_X_compute_edge_lengths(_, Scalar, Index)                       \
    template LA_CORE_API AttributeId compute_edge_lengths<Scalar, Index>( \
        SurfaceMesh<Scalar, Index>&,                                      \
        const EdgeLengthOptions&);
LA_SURFACE_MESH_X(compute_edge_lengths, 0)

} // namespace lagrange
