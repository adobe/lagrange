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
#include <lagrange/IndexedAttribute.h>
#include <lagrange/SurfaceMeshTypes.h>
#include <lagrange/compute_uv_distortion.h>
#include <lagrange/internal/find_attribute_utils.h>
#include <lagrange/utils/assert.h>
#include <lagrange/utils/triangle_uv_distortion.h>
#include <lagrange/views.h>

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <tbb/parallel_for.h>
#include <lagrange/utils/warnon.h>
// clang-format on

namespace lagrange {

template <typename Scalar, typename Index>
AttributeId compute_uv_distortion(
    SurfaceMesh<Scalar, Index>& mesh,
    const UVDistortionOptions& options)
{
    la_runtime_assert(mesh.is_triangle_mesh(), "Only triangle meshes are supported!");
    la_runtime_assert(mesh.get_dimension() == 3, "Only 3D meshes are supported!");
    la_runtime_assert(mesh.has_attribute(options.uv_attribute_name));
    la_runtime_assert(mesh.is_attribute_indexed(options.uv_attribute_name));

    AttributeId id = internal::find_or_create_attribute<Scalar>(
        mesh,
        options.output_attribute_name,
        Facet,
        AttributeUsage::Scalar,
        1,
        internal::ResetToDefault::No);

    const auto& uv_attr = mesh.template get_indexed_attribute<Scalar>(options.uv_attribute_name);
    auto uv_values = matrix_view(uv_attr.values());
    auto uv_indices = reshaped_view(uv_attr.indices(), 3);
    auto vertices = vertex_view(mesh);
    auto facets = facet_view(mesh);

    auto distortion_measure = vector_ref(mesh.template ref_attribute<Scalar>(id));
    la_debug_assert(distortion_measure.size() == facets.rows());

    Index num_facets = mesh.get_num_facets();
    tbb::parallel_for(Index(0), num_facets, [&](Index fid) {
        span<const Scalar, 3> V0{vertices.row(facets(fid, 0)).data(), 3};
        span<const Scalar, 3> V1{vertices.row(facets(fid, 1)).data(), 3};
        span<const Scalar, 3> V2{vertices.row(facets(fid, 2)).data(), 3};
        span<const Scalar, 2> v0{uv_values.row(uv_indices(fid, 0)).data(), 3};
        span<const Scalar, 2> v1{uv_values.row(uv_indices(fid, 1)).data(), 3};
        span<const Scalar, 2> v2{uv_values.row(uv_indices(fid, 2)).data(), 3};

        distortion_measure[fid] = triangle_uv_distortion(V0, V1, V2, v0, v1, v2, options.metric);
    });

    return id;
}

#define LA_X_compute_uv_distortion(_, Scalar, Index) \
    template LA_CORE_API AttributeId compute_uv_distortion(      \
        SurfaceMesh<Scalar, Index>&,                 \
        const UVDistortionOptions&);
LA_SURFACE_MESH_X(compute_uv_distortion, 0)

} // namespace lagrange
