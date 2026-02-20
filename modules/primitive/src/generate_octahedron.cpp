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
#include <lagrange/SurfaceMeshTypes.h>
#include <lagrange/compute_normal.h>
#include <lagrange/primitive/api.h>
#include <lagrange/primitive/generate_octahedron.h>
#include <lagrange/views.h>

#include "primitive_utils.h"

namespace lagrange::primitive {

template <typename Scalar, typename Index>
SurfaceMesh<Scalar, Index> generate_octahedron(OctahedronOptions setting)
{
    setting.project_to_valid_range();

    const Scalar r = setting.radius;
    SurfaceMesh<Scalar, Index> mesh;
    mesh.add_vertex({0, -r, 0});
    mesh.add_vertex({0, 0, r});
    mesh.add_vertex({-r, 0, 0});
    mesh.add_vertex({0, 0, -r});
    mesh.add_vertex({r, 0, 0});
    mesh.add_vertex({0, r, 0});

    // clnag-format off
    constexpr Index facets[] = {0, 1, 2, 0, 2, 3, 0, 3, 4, 0, 4, 1,
                                5, 2, 1, 5, 3, 2, 5, 4, 3, 5, 1, 4};
    // clang-format on
    mesh.add_triangles(8, {facets, 24});

    if (setting.normal_attribute_name != "") {
        NormalOptions normal_options;
        normal_options.output_attribute_name = setting.normal_attribute_name;
        normal_options.weight_type = NormalWeightingType::Uniform;
        const Scalar theta = static_cast<Scalar>(setting.angle_threshold);
        lagrange::compute_normal(mesh, theta, {}, normal_options);
    }

    if (setting.uv_attribute_name != "") {
        // Generate canonical UV coordinates using an octahedron net pattern with indexed attributes
        // Create a horizontal strip layout where triangles are arranged in a 4x2 configuration

        // Create UV attribute using indexed storage for efficiency
        auto uv_attr_id = mesh.template create_attribute<Scalar>(
            setting.uv_attribute_name,
            AttributeElement::Indexed,
            2,
            AttributeUsage::UV);
        auto& uv_attr = mesh.template ref_indexed_attribute<Scalar>(uv_attr_id);
        auto& uv_values = uv_attr.values();
        auto& uv_indices = uv_attr.indices();

        // UV coordinates for canonical octahedron net pattern in [0,1] range
        // Create a strip-like unfolding pattern that preserves adjacency where possible
        constexpr Scalar zero = static_cast<Scalar>(0.0);
        constexpr Scalar one_eighth = static_cast<Scalar>(0.125);
        constexpr Scalar quarter = static_cast<Scalar>(0.25);
        constexpr Scalar three_eighth = static_cast<Scalar>(0.375);
        constexpr Scalar half = static_cast<Scalar>(0.5);
        constexpr Scalar five_eighth = static_cast<Scalar>(0.625);
        constexpr Scalar three_quarter = static_cast<Scalar>(0.75);
        constexpr Scalar seven_eighth = static_cast<Scalar>(0.875);
        constexpr Scalar one = static_cast<Scalar>(1.0);
        constexpr Scalar sqrt3_8 = static_cast<Scalar>(0.21650635094610965); // sqrt(3)/8

        // Set up UV values
        uv_values.resize_elements(13);
        auto uvs_matrix = matrix_ref(uv_values);
        uvs_matrix <<
            // Middle row
            zero,
            half, // 0
            quarter, half, // 1
            half, half, // 2
            three_quarter, half, // 3
            one, half, // 4

            // Top row
            one_eighth, half + sqrt3_8, // 5
            three_eighth, half + sqrt3_8, // 6
            five_eighth, half + sqrt3_8, // 7
            seven_eighth, half + sqrt3_8, // 8

            // Bottom row
            one_eighth, half - sqrt3_8, // 9
            three_eighth, half - sqrt3_8, // 10
            five_eighth, half - sqrt3_8, // 11
            seven_eighth, half - sqrt3_8; // 12

        auto uv_indices_matrix = matrix_ref(uv_indices);
        // clang-format off
        uv_indices_matrix <<
            9, 1, 0,
            10, 2, 1,
            11, 3, 2,
            12, 4, 3,
            5, 0, 1,
            6, 1, 2,
            7, 2, 3,
            8, 3, 4;
        // clang-format on

        if (setting.fixed_uv) {
            normalize_uv(mesh, {0, 0}, {1, 1});
        }
    }

    if (setting.semantic_label_attribute_name != "") {
        add_semantic_label(mesh, setting.semantic_label_attribute_name, SemanticLabel::Side);
    }

    center_mesh(mesh, setting.center);
    return mesh;
}

#define LA_X_generate_octahedron(_, Scalar, Index)                                           \
    template LA_PRIMITIVE_API SurfaceMesh<Scalar, Index> generate_octahedron<Scalar, Index>( \
        OctahedronOptions);

LA_SURFACE_MESH_X(generate_octahedron, 0)

} // namespace lagrange::primitive
