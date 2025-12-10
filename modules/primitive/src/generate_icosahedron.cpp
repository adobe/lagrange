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
#include <lagrange/primitive/generate_icosahedron.h>
#include <lagrange/views.h>

#include "primitive_utils.h"

namespace lagrange::primitive {

template <typename Scalar, typename Index>
SurfaceMesh<Scalar, Index> generate_icosahedron(IcosahedronOptions setting)
{
    setting.project_to_valid_range();
    SurfaceMesh<Scalar, Index> mesh;

    // clang-format off
    constexpr Scalar ico_vertices[] = {
         0.000000, -1.000000,  0.000000,
         0.723600, -0.447215,  0.525720,
        -0.276385, -0.447215,  0.850640,
        -0.894425, -0.447215,  0.000000,
        -0.276385, -0.447215, -0.850640,
         0.723600, -0.447215, -0.525720,
         0.276385,  0.447215,  0.850640,
        -0.723600,  0.447215,  0.525720,
        -0.723600,  0.447215, -0.525720,
         0.276385,  0.447215, -0.850640,
         0.894425,  0.447215,  0.000000,
         0.000000,  1.000000,  0.000000,
    };
    // clang-format on

    // clang-format off
    constexpr Index ico_facets[] = {
         0,  1,  2,
         1,  0,  5,
         0,  2,  3,
         0,  3,  4,
         0,  4,  5,
         1,  5, 10,
         2,  1,  6,
         3,  2,  7,
         4,  3,  8,
         5,  4,  9,
         1, 10,  6,
         2,  6,  7,
         3,  7,  8,
         4,  8,  9,
         5,  9, 10,
         6, 10, 11,
         7,  6, 11,
         8,  7, 11,
         9,  8, 11,
        10,  9, 11,
    };
    // clang-format on

    // clang-format off
    constexpr Scalar uvs[] = {
        0.181819, 0.      ,
        0.272728, 0.157461,
        0.09091 , 0.157461,
        0.363637, 0.      ,
        0.454546, 0.157461,
        0.909091, 0.      ,
        1.      , 0.157461,
        0.818182, 0.157461,
        0.727273, 0.      ,
        0.636364, 0.157461,
        0.545455, 0.      ,
        0.363637, 0.314921,
        0.181819, 0.314921,
        0.909091, 0.314921,
        0.727273, 0.314921,
        0.545455, 0.314921,
        0.      , 0.314921,
        0.272728, 0.472382,
        0.09091 , 0.472382,
        0.818182, 0.472382,
        0.636364, 0.472382,
        0.454546, 0.472382,
    };
    // clang-format on

    // clang-format off
    constexpr Index uv_indices[] = {
        0,  1,  2,
        1,  3,  4,
        5,  6,  7,
        8,  7,  9,
        10,  9,  4,
        1,  4, 11,
        2,  1, 12,
        7,  6, 13,
        9,  7, 14,
        4,  9, 15,
        1, 11, 12,
        2, 12, 16,
        7, 13, 14,
        9, 14, 15,
        4, 15, 11,
        12, 11, 17,
        16, 12, 18,
        14, 13, 19,
        15, 14, 20,
        11, 15, 21,
    };
    // clang-format on

    mesh.add_vertices(12, {ico_vertices, 36});
    mesh.add_triangles(20, {ico_facets, 60});

    auto vertices = vertex_ref(mesh);
    vertices.rowwise().normalize();
    vertices *= setting.radius;

    if (setting.uv_attribute_name != "") {
        mesh.template create_attribute<Scalar>(
            setting.uv_attribute_name,
            AttributeElement::Indexed,
            2,
            AttributeUsage::UV,
            {uvs, 44},
            {uv_indices, 60});

        if (setting.fixed_uv) {
            normalize_uv(mesh, {0, 0}, {1, 1});
        }
    }

    if (setting.normal_attribute_name != "") {
        NormalOptions normal_options;
        normal_options.output_attribute_name = setting.normal_attribute_name;
        normal_options.weight_type = NormalWeightingType::Uniform;
        const Scalar theta = static_cast<Scalar>(setting.angle_threshold);
        lagrange::compute_normal(mesh, theta, {}, normal_options);
    }

    if (setting.semantic_label_attribute_name != "") {
        add_semantic_label(mesh, setting.semantic_label_attribute_name, SemanticLabel::Side);
    }

    center_mesh(mesh, setting.center);

    return mesh;
}

#define LA_X_generate_icosahedron(_, Scalar, Index)                                           \
    template LA_PRIMITIVE_API SurfaceMesh<Scalar, Index> generate_icosahedron<Scalar, Index>( \
        IcosahedronOptions);

LA_SURFACE_MESH_X(generate_icosahedron, 0)

} // namespace lagrange::primitive
