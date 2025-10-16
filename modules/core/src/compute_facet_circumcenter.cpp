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
#include <lagrange/api.h>
#include <lagrange/compute_facet_circumcenter.h>
#include <lagrange/internal/find_attribute_utils.h>
#include <lagrange/utils/assert.h>
#include <lagrange/utils/geometry2d.h>
#include <lagrange/utils/geometry3d.h>
#include <lagrange/utils/warning.h>
#include <lagrange/views.h>

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <tbb/parallel_for.h>
#include <lagrange/utils/warnon.h>
// clang-format on

namespace lagrange {

template <typename Scalar, typename Index>
AttributeId compute_facet_circumcenter(
    SurfaceMesh<Scalar, Index>& mesh,
    FacetCircumcenterOptions options)
{
    la_runtime_assert(mesh.is_triangle_mesh(), "Mesh must be a triangle mesh.");
    const auto dim = mesh.get_dimension();
    const auto num_facets = mesh.get_num_facets();

    AttributeId id = internal::find_or_create_attribute<Scalar>(
        mesh,
        options.output_attribute_name,
        Facet,
        AttributeUsage::Vector,
        dim,
        internal::ResetToDefault::No);

    auto centers = attribute_matrix_ref<Scalar>(mesh, id);
    auto vertices = vertex_view(mesh);

    if (dim == 2) {
        tbb::parallel_for(Index(0), num_facets, [&](Index fid) {
            const auto facet_vertices = mesh.get_facet_vertices(fid);
            span<const Scalar, 2> v0(vertices.row(facet_vertices[0]).data(), 2);
            span<const Scalar, 2> v1(vertices.row(facet_vertices[1]).data(), 2);
            span<const Scalar, 2> v2(vertices.row(facet_vertices[2]).data(), 2);
            auto r = triangle_circumcenter_2d<Scalar>(v0, v1, v2);
            centers.row(fid) << r[0], r[1];
        });
    } else if (dim == 3) {
        LA_IGNORE_ARRAY_BOUNDS_BEGIN
        tbb::parallel_for(Index(0), num_facets, [&](Index fid) {
            const auto facet_vertices = mesh.get_facet_vertices(fid);
            Eigen::Vector3<Scalar> v0 = vertices.row(facet_vertices[0]);
            Eigen::Vector3<Scalar> v1 = vertices.row(facet_vertices[1]);
            Eigen::Vector3<Scalar> v2 = vertices.row(facet_vertices[2]);
            centers.row(fid) = triangle_circumcenter_3d(v0, v1, v2);
        });
        LA_IGNORE_ARRAY_BOUNDS_END
    }

    return id;
}

#define LA_X_compute_facet_circumcenter(_, Scalar, Index)                       \
    template LA_CORE_API AttributeId compute_facet_circumcenter<Scalar, Index>( \
        SurfaceMesh<Scalar, Index>&,                                            \
        FacetCircumcenterOptions);
LA_SURFACE_MESH_X(compute_facet_circumcenter, 0)

} // namespace lagrange
