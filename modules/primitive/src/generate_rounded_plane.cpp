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
#include <lagrange/bvh/weld_vertices.h>
#include <lagrange/combine_meshes.h>
#include <lagrange/primitive/SemanticLabel.h>
#include <lagrange/primitive/api.h>
#include <lagrange/primitive/generate_rounded_plane.h>
#include <lagrange/transform_mesh.h>
#include <lagrange/triangulate_polygonal_facets.h>
#include <lagrange/utils/SmallVector.h>
#include <lagrange/utils/assert.h>
#include <lagrange/views.h>

#include "primitive_utils.h"

#include <Eigen/Core>
#include <Eigen/Geometry>

namespace lagrange::primitive {

namespace {

///
/// Create a rectangle mesh in the XY plane.
///
/// Note this function generates geometry only. Normal and UV attributes are not created.
///
/// @param x_min Minimum X coordinate.
/// @param x_max Maximum X coordinate.
/// @param y_min Minimum Y coordinate.
/// @param y_max Maximum Y coordinate.
/// @param x_segments Number of segments along the X direction.
/// @param y_segments Number of segments along the Y direction.
///
/// @return The generated rectangle mesh.
///
template <typename Scalar, typename Index>
SurfaceMesh<Scalar, Index> create_rectangle(
    Scalar x_min,
    Scalar x_max,
    Scalar y_min,
    Scalar y_max,
    Index x_segments,
    Index y_segments)
{
    la_runtime_assert(x_segments > 0, "x_segments must be positive.");
    la_runtime_assert(y_segments > 0, "y_segments must be positive.");

    SurfaceMesh<Scalar, Index> mesh;
    mesh.add_vertices((x_segments + 1) * (y_segments + 1));
    mesh.add_quads(x_segments * y_segments);

    auto vertices = vertex_ref(mesh);
    auto quads = facet_ref(mesh);

    for (Index i = 0; i <= y_segments; i++) {
        for (Index j = 0; j <= x_segments; j++) {
            Index vid = i * (x_segments + 1) + j;
            vertices(vid, 0) = x_min + (x_max - x_min) * j / x_segments;
            vertices(vid, 1) = y_min + (y_max - y_min) * i / y_segments;
            vertices(vid, 2) = 0;
        }
    }

    for (Index i = 0; i < y_segments; i++) {
        for (Index j = 0; j < x_segments; j++) {
            Index fid = i * x_segments + j;
            Index v0 = i * (x_segments + 1) + j;
            Index v1 = v0 + 1;
            Index v2 = v1 + (x_segments + 1);
            Index v3 = v0 + (x_segments + 1);
            quads.row(fid) << v0, v1, v2, v3;
        }
    }

    return mesh;
}

///
/// Create a quarter disc mesh in the XY plane.
///
/// The center of the disc is at the origin. The quarter disc is in the first quadrant.
///
/// @param radius Radius of the quarter disc.
/// @param radial_segments Number of segments along the arc.
///
/// @return The generated quarter disc mesh.
///
template <typename Scalar, typename Index>
SurfaceMesh<Scalar, Index> create_quarter_disc(Scalar radius, Index radial_segments)
{
    SurfaceMesh<Scalar, Index> mesh;
    mesh.add_vertices(radial_segments + 2);
    mesh.add_triangles(radial_segments);

    auto vertices = vertex_ref(mesh);
    auto facets = facet_ref(mesh);

    vertices.row(0) << 0, 0, 0; // center
    for (Index i = 0; i <= radial_segments; i++) {
        Scalar theta = static_cast<Scalar>(lagrange::internal::pi / 2) * i / radial_segments;
        vertices.row(i + 1) << radius * std::cos(theta), radius * std::sin(theta), 0;
    }
    for (Index i = 0; i < radial_segments; i++) {
        facets.row(i) << 0, i + 1, i + 2;
    }

    return mesh;
}

} // namespace

template <typename Scalar, typename Index>
SurfaceMesh<Scalar, Index> generate_rounded_plane(RoundedPlaneOptions setting)
{
    using AffineTransform = Eigen::Transform<Scalar, 3, Eigen::Affine>;

    setting.project_to_valid_range();
    SmallVector<SurfaceMesh<Scalar, Index>, 9> parts;

    Scalar half_width = setting.width / 2 - setting.bevel_radius;
    Scalar half_height = setting.height / 2 - setting.bevel_radius;

    // Center rectangle
    if (half_width > setting.epsilon && half_height > setting.epsilon) {
        auto center_rect = create_rectangle<Scalar, Index>(
            -half_width,
            half_width,
            -half_height,
            half_height,
            setting.width_segments,
            setting.height_segments);
        parts.push_back(std::move(center_rect));
    }

    if (setting.bevel_radius > setting.epsilon) {
        // 4 side rectangles
        {
            if (half_height > setting.epsilon) {
                auto left_rect = create_rectangle<Scalar, Index>(
                    -setting.width / 2,
                    -half_width,
                    -half_height,
                    half_height,
                    1,
                    setting.height_segments);
                auto right_rect = create_rectangle<Scalar, Index>(
                    half_width,
                    setting.width / 2,
                    -half_height,
                    half_height,
                    1,
                    setting.height_segments);

                parts.push_back(std::move(left_rect));
                parts.push_back(std::move(right_rect));
            }

            if (half_width > setting.epsilon) {
                auto top_rect = create_rectangle<Scalar, Index>(
                    -half_width,
                    half_width,
                    half_height,
                    setting.height / 2,
                    setting.width_segments,
                    1);
                auto bottom_rect = create_rectangle<Scalar, Index>(
                    -half_width,
                    half_width,
                    -setting.height / 2,
                    -half_height,
                    setting.width_segments,
                    1);

                parts.push_back(std::move(top_rect));
                parts.push_back(std::move(bottom_rect));
            }
        }

        // 4 rounded corners
        {
            AffineTransform rot90;
            rot90.setIdentity();
            rot90.rotate(
                Eigen::AngleAxis<Scalar>(
                    static_cast<Scalar>(lagrange::internal::pi / 2),
                    Eigen::Vector3<Scalar>::UnitZ()));
            AffineTransform translate;

            auto quarter_disc =
                create_quarter_disc<Scalar, Index>(setting.bevel_radius, setting.bevel_segments);

            translate.setIdentity();
            translate.translate(Eigen::Matrix<Scalar, 3, 1>(half_width, half_height, 0));
            auto top_right = transformed_mesh(quarter_disc, translate);

            translate.setIdentity();
            translate.translate(Eigen::Matrix<Scalar, 3, 1>(-half_width, half_height, 0));
            auto top_left = transformed_mesh(quarter_disc, translate * rot90);

            translate.setIdentity();
            translate.translate(Eigen::Matrix<Scalar, 3, 1>(half_width, -half_height, 0));
            auto bottom_right = transformed_mesh(quarter_disc, translate * rot90.inverse());

            translate.setIdentity();
            translate.translate(Eigen::Matrix<Scalar, 3, 1>(-half_width, -half_height, 0));
            auto bottom_left = transformed_mesh(quarter_disc, translate * rot90 * rot90);

            parts.push_back(std::move(top_right));
            parts.push_back(std::move(top_left));
            parts.push_back(std::move(bottom_right));
            parts.push_back(std::move(bottom_left));
        }
    }

    auto mesh = combine_meshes<Scalar, Index>({parts.data(), parts.size()}, false);

    // TODO: Add option to let user control whether to weld.
    bvh::WeldOptions weld_options;
    weld_options.boundary_only = true;
    weld_options.radius = setting.dist_threshold;
    bvh::weld_vertices(mesh, weld_options);

    if (setting.triangulate) {
        triangulate_polygonal_facets(mesh);
    }

    // Generate normals
    if (setting.normal_attribute_name != "") {
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
    }

    // Generate UV coordinates
    if (setting.uv_attribute_name != "") {
        auto vertices = vertex_view(mesh);

        auto uv_attr_id = mesh.template create_attribute<Scalar>(
            setting.uv_attribute_name,
            AttributeElement::Indexed,
            2,
            AttributeUsage::UV);
        auto& uv_attr = mesh.template ref_indexed_attribute<Scalar>(uv_attr_id);
        auto& uv_values = uv_attr.values();
        auto& uv_indices = uv_attr.indices();
        uv_values.resize_elements(mesh.get_num_vertices());

        auto uv_values_ref = matrix_ref(uv_values);
        auto uv_indices_ref = vector_ref(uv_indices);
        uv_values_ref = vertices.template leftCols<2>();
        uv_indices_ref = attribute_vector_view<Index>(mesh, mesh.attr_id_corner_to_vertex());

        if (setting.fixed_uv) {
            normalize_uv(mesh, {0, 0}, {1, 1});
        } else {
            // Normalize UV to fit in a [0, 1] box.
            uv_values_ref.col(0).array() += setting.width / 2;
            uv_values_ref.col(1).array() += setting.height / 2;
            if (setting.width > setting.epsilon || setting.height > setting.epsilon) {
                uv_values_ref /= std::max(setting.width, setting.height);
            }
        }
    }

    // Semantic label
    if (setting.semantic_label_attribute_name != "") {
        add_semantic_label(mesh, setting.semantic_label_attribute_name, SemanticLabel::Top);
    }

    // Reorient to align with the specified normal.
    {
        Eigen::Vector3<Scalar> from_vector(0, 0, 1);
        Eigen::Vector3<Scalar> to_vector(setting.normal[0], setting.normal[1], setting.normal[2]);

        AffineTransform rotation(Eigen::Quaternion<Scalar>::FromTwoVectors(from_vector, to_vector));
        transform_mesh(mesh, rotation);
    }

    center_mesh(mesh, setting.center);
    return mesh;
}

#define LA_X_generate_rounded_plane(_, Scalar, Index)                                           \
    template LA_PRIMITIVE_API SurfaceMesh<Scalar, Index> generate_rounded_plane<Scalar, Index>( \
        RoundedPlaneOptions);

LA_SURFACE_MESH_X(generate_rounded_plane, 0)

} // namespace lagrange::primitive
