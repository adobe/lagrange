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

#include <lagrange/Attribute.h>
#include <lagrange/SurfaceMeshTypes.h>
#include <lagrange/compute_area.h>
#include <lagrange/compute_centroid.h>
#include <lagrange/internal/find_attribute_utils.h>
#include <lagrange/utils/Error.h>
#include <lagrange/utils/assert.h>
#include <lagrange/views.h>

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <tbb/parallel_for.h>
#include <lagrange/utils/warnon.h>
// clang-format on

namespace lagrange {

template <typename Scalar, typename Index>
AttributeId compute_facet_centroid(SurfaceMesh<Scalar, Index>& mesh, FacetCentroidOptions options)
{
    const auto num_facets = mesh.get_num_facets();
    const auto dim = mesh.get_dimension();
    AttributeId id = internal::find_or_create_attribute<Scalar>(
        mesh,
        options.output_attribute_name,
        Facet,
        AttributeUsage::Vector,
        dim,
        internal::ResetToDefault::No);

    auto& attr = mesh.template ref_attribute<Scalar>(id);
    la_debug_assert(static_cast<Index>(attr.get_num_elements()) == num_facets);
    la_debug_assert(static_cast<Index>(attr.get_num_channels()) == dim);
    auto attr_ref = matrix_ref(attr);

    const auto& vertex_positions = vertex_view(mesh);
    if (mesh.is_triangle_mesh()) {
        tbb::parallel_for(Index(0), num_facets, [&](Index fid) {
            const auto facet_vertices = mesh.get_facet_vertices(fid);
            attr_ref.row(fid) =
                (vertex_positions.row(facet_vertices[0]) + vertex_positions.row(facet_vertices[1]) +
                 vertex_positions.row(facet_vertices[2])) /
                3;
        });
    } else {
        attr_ref.setZero();
        tbb::parallel_for(Index(0), num_facets, [&](Index fid) {
            const auto facet_vertices = mesh.get_facet_vertices(fid);
            const size_t n = facet_vertices.size();

            for (size_t i = 0; i < n; ++i) {
                attr_ref.row(fid) += vertex_positions.row(facet_vertices[i]);
            }
            attr_ref.row(fid) /= static_cast<Scalar>(n);
        });
    }

    return id;
}

template <typename Scalar, typename Index>
void compute_mesh_centroid(
    const SurfaceMesh<Scalar, Index>& mesh,
    span<Scalar> centroid,
    MeshCentroidOptions options)
{
    SurfaceMesh<Scalar, Index> working_mesh = mesh;
    const auto dim = working_mesh.get_dimension();
    la_runtime_assert(dim == 2 || dim == 3);
    la_runtime_assert(centroid.size() == dim);
    la_runtime_assert(mesh.get_num_vertices() > 0);
    switch (options.weighting_type) {
    case MeshCentroidOptions::Uniform: {
        const auto& vertices = vertex_view(working_mesh);
        const auto c = vertices.colwise().mean().eval();
        centroid[0] = c[0];
        centroid[1] = c[1];
        if (dim == 3) centroid[2] = c[2];
    } break;
    case MeshCentroidOptions::Area: {
        la_runtime_assert(mesh.get_num_facets() > 0);
        AttributeId centroid_id = invalid_attribute_id();
        if (!working_mesh.has_attribute(options.facet_centroid_attribute_name)) {
            FacetCentroidOptions fc_options;
            fc_options.output_attribute_name = options.facet_centroid_attribute_name;
            centroid_id = compute_facet_centroid(working_mesh, fc_options);
        } else {
            centroid_id = working_mesh.get_attribute_id(options.facet_centroid_attribute_name);
        }
        la_debug_assert(centroid_id != invalid_attribute_id());
        const auto& centroid_attr = working_mesh.template get_attribute<Scalar>(centroid_id);
        const auto centroids = matrix_view(centroid_attr);

        AttributeId area_id = invalid_attribute_id();
        if (!working_mesh.has_attribute(options.facet_area_attribute_name)) {
            FacetAreaOptions fa_options;
            fa_options.use_signed_area = false;
            fa_options.output_attribute_name = options.facet_area_attribute_name;
            area_id = compute_facet_area(working_mesh, fa_options);
        } else {
            area_id = working_mesh.get_attribute_id(options.facet_area_attribute_name);
        }
        la_debug_assert(area_id != invalid_attribute_id());
        const auto& area_attr = working_mesh.template get_attribute<Scalar>(area_id);
        const auto areas = vector_view(area_attr);
        const Scalar total_area = areas.array().sum();
        la_runtime_assert(total_area > 0, "Mesh must not have 0 total area.");

        auto c = ((areas.transpose() * centroids) / total_area).eval();
        la_runtime_assert(c.array().isFinite().all(), "Numerical error in centorid computation.");
        centroid[0] = c[0];
        centroid[1] = c[1];
        if (dim == 3) centroid[2] = c[2];
    } break;
    default: throw Error("Unsupported centroid weighting type.");
    }
}

#define LA_X_compute_centroid(_, Scalar, Index)                 \
    template LA_CORE_API AttributeId compute_facet_centroid<Scalar, Index>( \
        SurfaceMesh<Scalar, Index>&,                            \
        FacetCentroidOptions);                                  \
    template LA_CORE_API void compute_mesh_centroid<Scalar, Index>(         \
        const SurfaceMesh<Scalar, Index>&,                      \
        span<Scalar>,                                           \
        MeshCentroidOptions);
LA_SURFACE_MESH_X(compute_centroid, 0)

} // namespace lagrange
