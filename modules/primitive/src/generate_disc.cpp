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
#include <lagrange/internal/constants.h>
#include <lagrange/primitive/api.h>
#include <lagrange/primitive/generate_disc.h>
#include <lagrange/transform_mesh.h>
#include <lagrange/triangulate_polygonal_facets.h>
#include <lagrange/views.h>

#include "primitive_utils.h"

#include <Eigen/Geometry>

#include <cmath>

namespace lagrange::primitive {

template <typename Scalar, typename Index>
SurfaceMesh<Scalar, Index> generate_disc(DiscOptions setting)
{
    constexpr Scalar TWO_PI = static_cast<Scalar>(2 * lagrange::internal::pi);
    setting.project_to_valid_range();

    const Scalar angle_span = setting.end_angle - setting.start_angle;
    const bool is_closed =
        std::abs(std::abs(angle_span) - std::round(std::abs(angle_span) / TWO_PI) * TWO_PI) <
        setting.epsilon;
    const size_t vertices_per_ring =
        is_closed ? setting.radial_sections : setting.radial_sections + 1;
    const size_t num_rings = setting.num_rings;

    SurfaceMesh<Scalar, Index> mesh;
    mesh.add_vertices(vertices_per_ring * num_rings + 1);

    auto vertices = vertex_ref(mesh);

    // Populate vertices
    vertices.row(0).setZero(); // Center point at the origin
    for (size_t l = 0; l < num_rings; ++l) {
        Scalar r = setting.radius * static_cast<Scalar>(l + 1) / static_cast<Scalar>(num_rings);
        size_t offset = l * vertices_per_ring + 1; // +1 for the center vertex
        for (size_t i = 0; i < vertices_per_ring; ++i) {
            Scalar t = static_cast<Scalar>(i) / static_cast<Scalar>(setting.radial_sections);
            Scalar angle = setting.start_angle + angle_span * t;
            vertices.row(offset + i) << r * std::cos(angle), r * std::sin(angle), 0;
        }
    }

    // Populate center triangle fan
    {
        mesh.add_triangles(setting.radial_sections);
        auto facets = facet_ref(mesh);
        for (size_t i = 0; i < setting.radial_sections; ++i) {
            facets.row(i) << 0, static_cast<Index>(i + 1),
                static_cast<Index>((i + 1) % vertices_per_ring + 1);
        }
    }

    // Populate radial quads
    if (num_rings > 1) {
        for (size_t l = 1; l < num_rings; l++) {
            for (size_t i = 0; i < setting.radial_sections; i++) {
                Index v0 = static_cast<Index>(l * vertices_per_ring + i + 1);
                Index v1 =
                    static_cast<Index>(l * vertices_per_ring + (i + 1) % vertices_per_ring + 1);
                Index v2 = static_cast<Index>(((l - 1) * vertices_per_ring + i + 1));
                Index v3 = static_cast<Index>(
                    ((l - 1) * vertices_per_ring + (i + 1) % vertices_per_ring + 1));

                mesh.add_quad(v0, v1, v3, v2);
            }
        }
    }

    // Generate UV coordinates
    auto uv_attr_id = mesh.template create_attribute<Scalar>(
        setting.uv_attribute_name,
        AttributeElement::Indexed,
        2,
        AttributeUsage::UV);
    auto& uv_attr = mesh.template ref_indexed_attribute<Scalar>(uv_attr_id);
    auto& uv_values = uv_attr.values();
    auto& uv_indices = uv_attr.indices();
    uv_values.resize_elements(vertices_per_ring * num_rings + 1);

    if (!setting.fixed_uv) {
        matrix_ref(uv_values) = vertices.template leftCols<2>();
        vector_ref(uv_indices) =
            attribute_vector_view<Index>(mesh, mesh.attr_id_corner_to_vertex());
    } else {
        // Always map UVs to a complete disc
        auto uvs = matrix_ref(uv_values);
        uvs.row(0).setZero();
        for (size_t l = 0; l < num_rings; l++) {
            Scalar r = setting.radius * static_cast<Scalar>(l + 1) / static_cast<Scalar>(num_rings);
            size_t offset = l * vertices_per_ring + 1; // +1 for the center vertex
            for (size_t i = 0; i < vertices_per_ring; ++i) {
                Scalar t = static_cast<Scalar>(i) / static_cast<Scalar>(setting.radial_sections);
                Scalar angle = 2 * lagrange::internal::pi * t;
                uvs.row(offset + i) << r * std::cos(angle), r * std::sin(angle);
            }
        }
        vector_ref(uv_indices) =
            attribute_vector_view<Index>(mesh, mesh.attr_id_corner_to_vertex());
    }

    // Generate normals
    auto normal_attr_id = mesh.template create_attribute<Scalar>(
        setting.normal_attribute_name,
        AttributeElement::Indexed,
        3,
        AttributeUsage::Normal);
    auto& normal_attr = mesh.template ref_indexed_attribute<Scalar>(normal_attr_id);
    auto& normal_values = normal_attr.values();
    auto& normal_indices = normal_attr.indices();
    normal_values.resize_elements(1);

    matrix_ref(normal_values).row(0) << 0, 0, 1;
    vector_ref(normal_indices).setZero(); // All facets share the same normal

    if (setting.triangulate) {
        TriangulationOptions triangulation_options;
        triangulation_options.scheme = TriangulationOptions::Scheme::CentroidFan;
        triangulate_polygonal_facets(mesh, triangulation_options);
    }

    // Reorient to align with the specified normal.
    {
        using AffineTransform = Eigen::Transform<Scalar, 3, Eigen::Affine>;
        Eigen::Vector3<Scalar> from_vector(0, 0, 1);
        Eigen::Vector3<Scalar> to_vector(setting.normal[0], setting.normal[1], setting.normal[2]);

        AffineTransform rotation(Eigen::Quaternion<Scalar>::FromTwoVectors(from_vector, to_vector));
        transform_mesh(mesh, rotation);
    }

    center_mesh(mesh, setting.center);

    return mesh;
}

#define LA_X_generate_disc(_, Scalar, Index) \
    template LA_PRIMITIVE_API SurfaceMesh<Scalar, Index> generate_disc<Scalar, Index>(DiscOptions);

LA_SURFACE_MESH_X(generate_disc, 0)

} // namespace lagrange::primitive
