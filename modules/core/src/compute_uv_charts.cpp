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
#include <lagrange/Attribute.h>
#include <lagrange/IndexedAttribute.h>
#include <lagrange/SurfaceMeshTypes.h>
#include <lagrange/compute_components.h>
#include <lagrange/compute_uv_charts.h>
#include <lagrange/utils/Error.h>
#include <lagrange/utils/assert.h>
#include <lagrange/uv_mesh.h>

namespace lagrange {

template <typename Scalar, typename Index>
size_t compute_uv_charts(SurfaceMesh<Scalar, Index>& mesh, const UVChartOptions& options)
{
    UVMeshOptions uv_mesh_options;
    uv_mesh_options.uv_attribute_name = options.uv_attribute_name;

    ComponentOptions component_options;
    component_options.connectivity_type = options.connectivity_type;
    component_options.output_attribute_name = options.output_attribute_name;

    using OtherScalar = std::conditional_t<std::is_same_v<Scalar, float>, double, float>;

    size_t num_charts;
    if (uv_attribute_id<Scalar, Index, Scalar>(mesh, uv_mesh_options)) {
        auto uv_mesh = uv_mesh_view<Scalar, Index, Scalar>(mesh, uv_mesh_options);
        num_charts = compute_components(uv_mesh, component_options);
        mesh.create_attribute_from(options.output_attribute_name, uv_mesh);
    } else if (uv_attribute_id<Scalar, Index, OtherScalar>(mesh, uv_mesh_options)) {
        auto uv_mesh = uv_mesh_view<Scalar, Index, OtherScalar>(mesh, uv_mesh_options);
        num_charts = compute_components(uv_mesh, component_options);
        mesh.create_attribute_from(options.output_attribute_name, uv_mesh);
    } else {
        throw Error("compute_uv_charts: no suitable UV attribute found.");
    }

    return num_charts;
}

#define LA_X_compute_uv_charts(_, Scalar, Index)                  \
    template LA_CORE_API size_t compute_uv_charts<Scalar, Index>( \
        SurfaceMesh<Scalar, Index>&,                              \
        const UVChartOptions&);
LA_SURFACE_MESH_X(compute_uv_charts, 0)
} // namespace lagrange
