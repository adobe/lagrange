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
#include <lagrange/bvh/weld_vertices.h>
#include <lagrange/combine_meshes.h>
#include <lagrange/mesh_cleanup/remove_degenerate_facets.h>
#include <lagrange/mesh_cleanup/remove_duplicate_vertices.h>
#include <lagrange/mesh_cleanup/remove_short_edges.h>
#include <lagrange/packing/repack_uv_charts.h>
#include <lagrange/primitive/SemanticLabel.h>
#include <lagrange/primitive/SweepOptions.h>
#include <lagrange/primitive/api.h>
#include <lagrange/primitive/generate_disc.h>
#include <lagrange/primitive/generate_rounded_cone.h>
#include <lagrange/primitive/generate_swept_surface.h>
#include <lagrange/transform_mesh.h>
#include <lagrange/triangulate_polygonal_facets.h>
#include <lagrange/utils/SmallVector.h>
#include <lagrange/utils/assert.h>
#include <lagrange/weld_indexed_attribute.h>

#include "primitive_utils.h"

#include <lagrange/internal/constants.h>
#include <Eigen/Core>

namespace lagrange::primitive {

namespace {

template <typename Scalar>
auto generate_profile(const RoundedConeOptions& setting)
    -> std::tuple<Eigen::Matrix<Scalar, Eigen::Dynamic, 2, Eigen::RowMajor>, size_t, size_t>
{
    using Point = Eigen::Matrix<Scalar, 1, 2>;
    using PointArray = Eigen::Matrix<Scalar, Eigen::Dynamic, 2, Eigen::RowMajor>;

    size_t num_total_segments =
        setting.bevel_segments_top + setting.bevel_segments_bottom + setting.side_segments;
    PointArray profile(num_total_segments + 1, 2);

    la_runtime_assert(setting.height > 0, "Height must be positive.");

    Scalar psi = std::atan((setting.radius_top - setting.radius_bottom) / setting.height);
    Scalar a1 = (Scalar)(lagrange::internal::pi_2 + psi) * .5f; // Bottom half angle
    Scalar a2 = (Scalar)(lagrange::internal::pi_2 - psi) * .5f; // Top half angle

    Scalar bottom_bevel_length = setting.bevel_radius_bottom / std::tan(a1);
    Scalar top_bevel_length = setting.bevel_radius_top / std::tan(a2);
    Scalar bottom_length = setting.radius_bottom - bottom_bevel_length;
    Scalar top_length = setting.radius_top - top_bevel_length;
    Scalar side_length = setting.height - bottom_bevel_length - top_bevel_length;

    bottom_length = std::max(bottom_length, Scalar(0));
    top_length = std::max(top_length, Scalar(0));
    side_length = std::max(side_length, Scalar(0));

    size_t row_index = 0;
    size_t side_start_index = 0;
    size_t side_end_index = 0;
    profile.row(row_index) << bottom_length, 0;

    // Bottom bevel segments
    if (setting.bevel_radius_bottom > setting.epsilon && setting.bevel_segments_bottom > 0) {
        Scalar bottom_bevel_angle = lagrange::internal::pi - a1 * 2;
        for (size_t i = 1; i <= setting.bevel_segments_bottom; i++) {
            Scalar t = static_cast<Scalar>(i) / setting.bevel_segments_bottom;
            Scalar theta = lagrange::internal::pi * 1.5 + bottom_bevel_angle * t;
            profile.row(row_index + i)
                << setting.bevel_radius_bottom * std::cos(theta) + bottom_length,
                setting.bevel_radius_bottom * std::sin(theta) + setting.bevel_radius_bottom;
        }
        row_index += setting.bevel_segments_bottom;
    }
    side_start_index = row_index;

    // Side segments
    if (side_length > setting.epsilon) {
        Point p0(setting.radius_bottom, 0);
        Point p1(setting.radius_top, setting.height);
        Point dir = (p1 - p0).stableNormalized();
        p0 += dir * bottom_bevel_length;
        p1 -= dir * top_bevel_length;

        for (size_t i = 1; i <= setting.side_segments; i++) {
            Scalar t = static_cast<Scalar>(i) / setting.side_segments;
            profile.row(row_index + i) = p0 + (p1 - p0) * t;
        }
        row_index += setting.side_segments;
    }
    side_end_index = row_index;

    // Top bevel segments
    if (setting.bevel_radius_top > setting.epsilon && setting.bevel_segments_top > 0) {
        Scalar top_bevel_angle = lagrange::internal::pi - a2 * 2;
        for (size_t i = 1; i <= setting.bevel_segments_top; i++) {
            Scalar t = static_cast<Scalar>(i) / setting.bevel_segments_top;
            Scalar theta = lagrange::internal::pi * 0.5 - top_bevel_angle * (1 - t);
            profile.row(row_index + i) << setting.bevel_radius_top * std::cos(theta) + top_length,
                setting.bevel_radius_top * std::sin(theta) + setting.height -
                    setting.bevel_radius_top;
        }
        row_index += setting.bevel_segments_top;
    }

    profile.conservativeResize(row_index + 1, 2);

    return {profile, side_start_index, side_end_index};
}

///
/// Extract vertices that are close to the symmetry axis (Y-axis).
///
/// @param mesh The surface mesh.
/// @param eps The distance threshold to consider a vertex as part of the cone.
///
/// @return A vector of vertex indices that are close to the symmetry axis.
///
template <typename Scalar, typename Index>
SmallVector<size_t, 2> extract_cone_vertices(const SurfaceMesh<Scalar, Index>& mesh, Scalar eps)
{
    SmallVector<size_t, 2> cone_vertices;
    auto vertices = vertex_view(mesh);
    Index num_vertices = mesh.get_num_vertices();

    for (Index i = 0; i < num_vertices; i++) {
        if (std::abs(vertices(i, 0)) < eps && std::abs(vertices(i, 2)) < eps) {
            cone_vertices.push_back(static_cast<size_t>(i));
        }
    }
    return cone_vertices;
}

} // namespace

template <typename Scalar, typename Index>
SurfaceMesh<Scalar, Index> generate_rounded_cone(RoundedConeOptions setting)
{
    using AffineTransform = Eigen::Transform<Scalar, 3, Eigen::Affine>;
    setting.project_to_valid_range();

    if (setting.height == 0 || (setting.radius_top == 0 && setting.radius_bottom == 0)) {
        logger().warn(
            "generate_rounded_cone(): Height is zero or both top and bottom radius are "
            "zero. Returning an empty mesh.");
        return SurfaceMesh<Scalar, Index>();
    }

    SmallVector<SurfaceMesh<Scalar, Index>, 7> parts;
    bool is_closed = true;
    AffineTransform transform_begin;
    AffineTransform transform_end;

    Eigen::Matrix<Scalar, Eigen::Dynamic, 2, Eigen::RowMajor> profile;
    size_t side_start_index = 0;
    size_t side_end_index = 0;
    std::tie(profile, side_start_index, side_end_index) = generate_profile<Scalar>(setting);
    auto profile_length = [&](size_t start_idx, size_t end_idx) {
        Scalar length = 0;
        for (size_t i = start_idx + 1; i <= end_idx; i++) {
            length += (profile.row(i) - profile.row(i - 1)).stableNorm();
        }
        return length;
    };

    // Generate side
    {
        bool with_bottom_bevel = side_start_index > 0;
        bool with_top_bevel = side_end_index < static_cast<size_t>(profile.rows() - 1);
        bool with_side = side_start_index < side_end_index;

        Scalar top_bevel_arc_length = profile_length(side_end_index, profile.rows() - 1);
        Scalar bottom_bevel_arc_length = profile_length(0, side_start_index);
        Scalar side_length = profile_length(side_start_index, side_end_index);
        Scalar total_length = top_bevel_arc_length + bottom_bevel_arc_length + side_length;

        Scalar average_radius = (setting.radius_top + setting.radius_bottom) / 2;
        auto sweep_setting = SweepOptions<Scalar>::circular_sweep(
            {average_radius, 0, 0}, // A point on the center line of the cone
            {0, -1, 0} // Axis of rotation
        );
        sweep_setting.set_num_samples(setting.radial_sections + 1);

        // Set pivot (i.e. relative origin) at a point on the unit circle
        sweep_setting.set_pivot({average_radius, 0, 0});

        // Set domain
        Scalar t_begin =
            static_cast<Scalar>(setting.start_sweep_angle / (2 * lagrange::internal::pi));
        Scalar t_end = static_cast<Scalar>(setting.end_sweep_angle / (2 * lagrange::internal::pi));
        sweep_setting.set_domain({t_begin, t_end});

        // Set sweep options
        SweptSurfaceOptions sweep_options;
        sweep_options.uv_attribute_name = setting.uv_attribute_name;
        sweep_options.normal_attribute_name = setting.normal_attribute_name;

        sweep_options.triangulate = setting.triangulate;
        sweep_options.angle_threshold = setting.angle_threshold;
        sweep_options.profile_angle_threshold = setting.angle_threshold;
        sweep_options.use_u_as_profile_length = false;

        sweep_options.longitude_attribute_name = "";
        sweep_options.latitude_attribute_name = "";

        // The body mesh is centered at the pivot point, moving it back to the origin.
        AffineTransform transform;
        transform.setIdentity();
        transform.translate(Eigen::Matrix<Scalar, 3, 1>(-average_radius, 0, 0));

        is_closed = sweep_setting.is_closed();
        transform_begin = transform * sweep_setting.sample_transform(t_begin);
        transform_end = transform * sweep_setting.sample_transform(t_end);

        if (with_bottom_bevel) {
            auto bottom_bevel = generate_swept_surface<Scalar, Index>(
                {profile.data(), (side_start_index + 1) * 2},
                sweep_setting,
                sweep_options);
            transform_mesh(bottom_bevel, transform);
            if (setting.fixed_uv) {
                Scalar max_v = 0.5 * (bottom_bevel_arc_length / total_length);
                normalize_uv(bottom_bevel, {0, 0}, {0.5, max_v});
            }

            add_semantic_label(
                bottom_bevel,
                setting.semantic_label_attribute_name,
                SemanticLabel::Bevel);
            parts.push_back(std::move(bottom_bevel));
        }
        if (with_side) {
            auto side = generate_swept_surface<Scalar, Index>(
                {profile.data() + side_start_index * 2,
                 (side_end_index - side_start_index + 1) * 2},
                sweep_setting,
                sweep_options);
            transform_mesh(side, transform);
            if (setting.fixed_uv) {
                Scalar min_v = 0.5 * (bottom_bevel_arc_length / total_length);
                Scalar max_v = 0.5 * ((bottom_bevel_arc_length + side_length) / total_length);
                normalize_uv(side, {0, min_v}, {0.5, max_v});
            }
            add_semantic_label(side, setting.semantic_label_attribute_name, SemanticLabel::Side);
            parts.push_back(std::move(side));
        }
        if (with_top_bevel) {
            auto top_bevel = generate_swept_surface<Scalar, Index>(
                {profile.data() + side_end_index * 2, (profile.rows() - side_end_index) * 2},
                sweep_setting,
                sweep_options);
            transform_mesh(top_bevel, transform);
            if (setting.fixed_uv) {
                Scalar min_v = 0.5 * ((bottom_bevel_arc_length + side_length) / total_length);
                normalize_uv(top_bevel, {0, min_v}, {0.5, 0.5});
            }
            add_semantic_label(
                top_bevel,
                setting.semantic_label_attribute_name,
                SemanticLabel::Bevel);
            parts.push_back(std::move(top_bevel));
        }
    }

    // Generate top cap
    if (setting.with_top_cap && profile(profile.rows() - 1, 0) > setting.epsilon) {
        DiscOptions disc_setting;
        disc_setting.radius = profile(profile.rows() - 1, 0);
        disc_setting.start_angle = 2 * lagrange::internal::pi - setting.end_sweep_angle;
        disc_setting.end_angle = 2 * lagrange::internal::pi - setting.start_sweep_angle;
        disc_setting.num_rings = setting.top_segments;
        disc_setting.radial_sections = setting.radial_sections;
        disc_setting.fixed_uv = setting.fixed_uv;
        disc_setting.triangulate = setting.triangulate;
        auto disc = generate_disc<Scalar, Index>(disc_setting);

        AffineTransform transform;
        transform.setIdentity();
        transform.rotate(
            Eigen::AngleAxis<Scalar>(
                static_cast<Scalar>(-lagrange::internal::pi / 2),
                Eigen::Matrix<Scalar, 3, 1>::UnitX()));
        transform.translate(Eigen::Matrix<Scalar, 3, 1>(0, 0, setting.height));
        auto top_cap = transformed_mesh(disc, transform);

        if (setting.fixed_uv) {
            normalize_uv(
                top_cap,
                {setting.uv_padding, 0.5 + setting.uv_padding},
                {0.5 - setting.uv_padding, 1 - setting.uv_padding});
        }

        add_semantic_label(top_cap, setting.semantic_label_attribute_name, SemanticLabel::Top);
        parts.push_back(std::move(top_cap));
    }

    // Generate bottom cap
    if (setting.with_bottom_cap && profile(0, 0) > setting.epsilon) {
        DiscOptions disc_setting;
        disc_setting.radius = profile(0, 0);
        disc_setting.start_angle = setting.start_sweep_angle;
        disc_setting.end_angle = setting.end_sweep_angle;
        disc_setting.num_rings = setting.bottom_segments;
        disc_setting.radial_sections = setting.radial_sections;
        disc_setting.fixed_uv = setting.fixed_uv;
        disc_setting.triangulate = setting.triangulate;
        auto disc = generate_disc<Scalar, Index>(disc_setting);

        AffineTransform transform;
        transform.setIdentity();
        transform.rotate(
            Eigen::AngleAxis<Scalar>(
                static_cast<Scalar>(lagrange::internal::pi / 2),
                Eigen::Matrix<Scalar, 3, 1>::UnitX()));
        auto bottom_cap = transformed_mesh(disc, transform);

        if (setting.fixed_uv) {
            normalize_uv(
                bottom_cap,
                {0.5 + setting.uv_padding, 0.5 + setting.uv_padding},
                {1 - setting.uv_padding, 1 - setting.uv_padding});
        }

        add_semantic_label(
            bottom_cap,
            setting.semantic_label_attribute_name,
            SemanticLabel::Bottom);
        parts.push_back(std::move(bottom_cap));
    }

    // Generate cross sections if needed
    if (setting.with_cross_section && !is_closed) {
        // Generate cross section
        Eigen::Matrix<Scalar, Eigen::Dynamic, 2, Eigen::RowMajor> cross_section_profile(
            profile.rows() + 2,
            2);
        cross_section_profile.row(0).setZero();
        cross_section_profile.middleRows(1, profile.rows()) = profile;
        cross_section_profile.row(profile.rows() + 1) << 0, setting.height;

        auto profile_mesh = boundary_to_mesh<Scalar, Index>(
            {cross_section_profile.data(), static_cast<size_t>(cross_section_profile.size())},
            setting.uv_attribute_name,
            setting.normal_attribute_name,
            false);
        auto flipped_profile_mesh = boundary_to_mesh<Scalar, Index>(
            {cross_section_profile.data(), static_cast<size_t>(cross_section_profile.size())},
            setting.uv_attribute_name,
            setting.normal_attribute_name,
            true);
        la_debug_assert(
            profile_mesh.get_num_vertices() == static_cast<Index>(cross_section_profile.rows()));
        la_debug_assert(profile_mesh.get_num_facets() == 1);
        la_debug_assert(
            flipped_profile_mesh.get_num_vertices() ==
            static_cast<Index>(cross_section_profile.rows()));
        la_debug_assert(flipped_profile_mesh.get_num_facets() == 1);

        if (setting.triangulate) {
            TriangulationOptions triangulation_options;
            triangulation_options.scheme = TriangulationOptions::Scheme::CentroidFan;
            triangulate_polygonal_facets(profile_mesh, triangulation_options);
            triangulate_polygonal_facets(flipped_profile_mesh, triangulation_options);
        }

        auto cross_section_begin = transformed_mesh(flipped_profile_mesh, transform_begin);
        auto cross_section_end = transformed_mesh(profile_mesh, transform_end);

        if (setting.fixed_uv) {
            normalize_uv(
                cross_section_begin,
                {0.5 + setting.uv_padding, setting.uv_padding},
                {0.75, 0.5 - setting.uv_padding});
            normalize_uv(
                cross_section_end,
                {0.75, setting.uv_padding},
                {1 - setting.uv_padding, 0.5 - setting.uv_padding});
        }

        add_semantic_label(
            cross_section_begin,
            setting.semantic_label_attribute_name,
            SemanticLabel::CrossSection);
        add_semantic_label(
            cross_section_end,
            setting.semantic_label_attribute_name,
            SemanticLabel::CrossSection);
        parts.push_back(std::move(cross_section_begin));
        parts.push_back(std::move(cross_section_end));
    }

    auto mesh = combine_meshes<Scalar, Index>({parts.data(), parts.size()}, true);

    // TODO: Add option to let user control whether to weld.
    bvh::WeldOptions weld_options;
    weld_options.boundary_only = true;
    weld_options.radius = setting.dist_threshold;
    bvh::weld_vertices(mesh, weld_options);

    auto cone_vertices = extract_cone_vertices(mesh, static_cast<Scalar>(setting.dist_threshold));

    // Weld indexed normals
    WeldOptions attr_weld_options;
    attr_weld_options.epsilon_abs = 1; // Disable distance-based check
    attr_weld_options.angle_abs = setting.angle_threshold;
    attr_weld_options.exclude_vertices = {cone_vertices.data(), cone_vertices.size()};
    weld_indexed_attribute(
        mesh,
        mesh.get_attribute_id(setting.normal_attribute_name),
        attr_weld_options);

    if (setting.triangulate) {
        remove_degenerate_facets(mesh);
    }

    if (!setting.fixed_uv) {
        packing::RepackOptions repack_options;
        repack_options.margin = setting.uv_padding;
        packing::repack_uv_charts(mesh);
    }

    // Translate the mesh so that the origin is at the center of the cone.
    {
        AffineTransform transform;
        transform.setIdentity();
        transform.translate(Eigen::Matrix<Scalar, 3, 1>(0, -setting.height / 2, 0));
        transform_mesh(mesh, transform);
    }

    center_mesh(mesh, setting.center);
    return mesh;
}

#define LA_X_generate_rounded_cone(_, Scalar, Index)                                           \
    template LA_PRIMITIVE_API SurfaceMesh<Scalar, Index> generate_rounded_cone<Scalar, Index>( \
        RoundedConeOptions);

LA_SURFACE_MESH_X(generate_rounded_cone, 0)

} // namespace lagrange::primitive
