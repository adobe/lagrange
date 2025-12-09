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

#include <lagrange/IndexedAttribute.h>
#include <lagrange/Logger.h>
#include <lagrange/SurfaceMeshTypes.h>
#include <lagrange/compute_normal.h>
#include <lagrange/primitive/api.h>
#include <lagrange/primitive/generate_subdivided_sphere.h>
#include <lagrange/subdivision/mesh_subdivision.h>
#include <lagrange/utils/assert.h>
#include <lagrange/views.h>

#include "primitive_utils.h"

namespace lagrange::primitive {

template <typename Scalar, typename Index>
SurfaceMesh<Scalar, Index> generate_subdivided_sphere(
    const SurfaceMesh<Scalar, Index>& base_shape,
    SubdividedSphereOptions setting)
{
    subdivision::SubdivisionOptions subdiv_options;
    subdiv_options.num_levels = setting.subdiv_level;
    if (base_shape.is_quad_mesh()) {
        subdiv_options.scheme = subdivision::SchemeType::CatmullClark;
    } else {
        subdiv_options.scheme = subdivision::SchemeType::Loop;
    }

    if (setting.uv_attribute_name != "") {
        la_runtime_assert(
            base_shape.has_attribute(setting.uv_attribute_name),
            fmt::format(
                "UV attribute '{}' not found in the base shape.",
                setting.uv_attribute_name));
        la_runtime_assert(
            base_shape.is_attribute_indexed(setting.uv_attribute_name),
            fmt::format("UV attribute '{}' must be indexed.", setting.uv_attribute_name));
        subdiv_options.face_varying_interpolation = subdivision::FaceVaryingInterpolation::All;
    }

    auto mesh = subdivision::subdivide_mesh(base_shape, subdiv_options);

    auto vertices = vertex_ref(mesh);
    vertices.rowwise().normalize();
    vertices *= setting.radius;

    if (setting.normal_attribute_name != "") {
        NormalOptions normal_options;
        normal_options.output_attribute_name = setting.normal_attribute_name;
        normal_options.weight_type = NormalWeightingType::Uniform;
        const Scalar theta = static_cast<Scalar>(setting.angle_threshold);
        lagrange::compute_normal(mesh, theta, {}, normal_options);
    }

    if (setting.uv_attribute_name != "") {
        if (setting.fixed_uv) {
            normalize_uv(mesh, {0, 0}, {1, 1});
        }
    }

    if (setting.semantic_label_attribute_name != "") {
        if (!mesh.has_attribute(setting.semantic_label_attribute_name)) {
            add_semantic_label(mesh, setting.semantic_label_attribute_name, SemanticLabel::Side);
        }
    }

    center_mesh(mesh, setting.center);
    return mesh;
}

#define LA_X_generate_subdivided_sphere(_, Scalar, Index) \
    template LA_PRIMITIVE_API SurfaceMesh<Scalar, Index>  \
    generate_subdivided_sphere<Scalar, Index>(            \
        const SurfaceMesh<Scalar, Index>& base_shape,     \
        SubdividedSphereOptions);

LA_SURFACE_MESH_X(generate_subdivided_sphere, 0)

} // namespace lagrange::primitive
