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
#include <lagrange/Logger.h>
#include <lagrange/SurfaceMeshTypes.h>
#include <lagrange/compute_facet_normal.h>
#include <lagrange/internal/find_attribute_utils.h>
#include <lagrange/internal/string_from_scalar.h>
#include <lagrange/utils/assert.h>
#include <lagrange/views.h>

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <tbb/parallel_for.h>
#include <lagrange/utils/warnon.h>
// clang-format on

#include <Eigen/Dense>

namespace lagrange {

template <typename Scalar, typename Index>
AttributeId compute_facet_normal(SurfaceMesh<Scalar, Index>& mesh, FacetNormalOptions options)
{
    la_runtime_assert(mesh.get_dimension() == 3, "Only 3D mesh is supported.");
    const auto num_facets = mesh.get_num_facets();

    AttributeId id = internal::find_or_create_attribute<Scalar>(
        mesh,
        options.output_attribute_name,
        Facet,
        AttributeUsage::Normal,
        3,
        internal::ResetToDefault::No);

    auto& attr = mesh.template ref_attribute<Scalar>(id);
    la_debug_assert(static_cast<Index>(attr.get_num_elements()) == num_facets);
    auto attr_ref = attr.ref_all(); // Just to trigger copy-on-write.

    const auto& vertex_positions = vertex_view(mesh);
    tbb::parallel_for(Index(0), num_facets, [&](Index fid) {
        // Robust polygon normal calculation
        // n ~ p0 x p1 + p1 x p2 + ... + pn x p0
        const auto facet_vertices = mesh.get_facet_vertices(fid);
        Eigen::Vector3<Scalar> normal = Eigen::Vector3<Scalar>::Zero();
        for (size_t i = 0, m = facet_vertices.size(); i < m; ++i) {
            size_t j = (i + 1) % m;
            normal += vertex_positions.row(facet_vertices[i])
                          .template head<3>()
                          .cross(vertex_positions.row(facet_vertices[j]).template head<3>());
        }
        normal.stableNormalize();
        attr_ref[fid * 3] = normal[0];
        attr_ref[fid * 3 + 1] = normal[1];
        attr_ref[fid * 3 + 2] = normal[2];
    });

    return id;
}

#define LA_X_compute_facet_normal(_, Scalar, Index) \
    template AttributeId compute_facet_normal(SurfaceMesh<Scalar, Index>& mesh, FacetNormalOptions);
LA_SURFACE_MESH_X(compute_facet_normal, 0)

} // namespace lagrange
