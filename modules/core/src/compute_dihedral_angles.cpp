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
#include <lagrange/compute_dihedral_angles.h>
#include <lagrange/internal/constants.h>
#include <lagrange/internal/find_attribute_utils.h>
#include <lagrange/utils/assert.h>
#include <lagrange/utils/geometry3d.h>
#include <lagrange/utils/invalid.h>
#include <lagrange/views.h>

#include "internal/recompute_facet_normal_if_needed.h"

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <tbb/parallel_for.h>
#include <lagrange/utils/warnon.h>
// clang-format on

namespace lagrange {

template <typename Scalar, typename Index>
AttributeId compute_dihedral_angles(
    SurfaceMesh<Scalar, Index>& mesh,
    const DihedralAngleOptions& options)
{
    mesh.initialize_edges();

    auto [facet_normal_id, had_facet_normals] = internal::recompute_facet_normal_if_needed(
        mesh,
        options.facet_normal_attribute_name,
        options.recompute_facet_normals);
    auto facet_normal = attribute_matrix_view<Scalar>(mesh, facet_normal_id);

    AttributeId attr_id = internal::find_or_create_attribute<Scalar>(
        mesh,
        options.output_attribute_name,
        Edge,
        AttributeUsage::Scalar,
        1,
        internal::ResetToDefault::Yes);
    auto dihedral_angles = attribute_matrix_ref<Scalar>(mesh, attr_id);

    const auto num_edges = mesh.get_num_edges();
    tbb::parallel_for((Index)0, num_edges, [&](Index ei) {
        Index c0 = mesh.get_first_corner_around_edge(ei);
        la_debug_assert(c0 != invalid<Index>());
        Index c1 = mesh.get_next_corner_around_edge(c0);
        if (c1 == invalid<Index>()) {
            // Boundary edge, default to 0.
            dihedral_angles(ei) = 0;
            return;
        }
        Index c2 = mesh.get_next_corner_around_edge(c1);
        if (c2 != invalid<Index>()) {
            // Non-manifold edge, default to 2 * π.
            dihedral_angles(ei) = static_cast<Scalar>(2 * lagrange::internal::pi);
            return;
        }

        // Manifold edge.
        Index f0 = mesh.get_corner_facet(c0);
        Index f1 = mesh.get_corner_facet(c1);
        const Eigen::Matrix<Scalar, 1, 3> n0 = facet_normal.row(f0);
        const Eigen::Matrix<Scalar, 1, 3> n1 = facet_normal.row(f1);
        dihedral_angles(ei) = angle_between(n0, n1);
    });

    if (!options.keep_facet_normals && !had_facet_normals) {
        mesh.delete_attribute(options.facet_normal_attribute_name);
    }
    return attr_id;
}

#define LA_X_compute_dihedral_angles(_, Scalar, Index)                       \
    template LA_CORE_API AttributeId compute_dihedral_angles<Scalar, Index>( \
        SurfaceMesh<Scalar, Index>&,                                         \
        const DihedralAngleOptions&);
LA_SURFACE_MESH_X(compute_dihedral_angles, 0)

} // namespace lagrange
