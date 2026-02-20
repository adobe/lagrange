/*
 * Copyright 2019 Adobe. All rights reserved.
 * This file is licensed to you under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License. You may obtain a copy
 * of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under
 * the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
 * OF ANY KIND, either express or implied. See the License for the specific language
 * governing permissions and limitations under the License.
 */
#pragma once

#include <algorithm>
#include <limits>
#include <vector>

#include <lagrange/Logger.h>
#include <lagrange/Mesh.h>
#include <lagrange/attributes/attribute_utils.h>
#include <lagrange/bvh/zip_boundary.h>
#include <lagrange/combine_mesh_list.h>
#include <lagrange/compute_normal.h>
#include <lagrange/create_mesh.h>
#include <lagrange/internal/constants.h>
#include <lagrange/legacy/inline.h>
#include <lagrange/mesh_cleanup/remove_degenerate_triangles.h>
#include <lagrange/packing/compute_rectangle_packing.h>
#include <lagrange/primitive/generation_utils.h>


namespace lagrange {
namespace primitive {
LAGRANGE_LEGACY_INLINE
namespace legacy {

namespace {

template <typename ScalarT>
std::pair<ScalarT, ScalarT>
get_max_cone_bevel(ScalarT radius_top, ScalarT radius_bottom, ScalarT height)
{
    // angle between the cone slope and the vertical line (0 for cylinders)
    ScalarT psi = std::atan((radius_top - radius_bottom) / height);
    ScalarT a1 = (ScalarT)(lagrange::internal::pi_2 + psi) * .5f;
    ScalarT a2 = (ScalarT)(lagrange::internal::pi_2 - psi) * .5f;

    ScalarT max_bevel_bottom = radius_bottom * std::tan(a1); // max bevel against radius and slope
    max_bevel_bottom = std::min(height * .5f, max_bevel_bottom); // also check against height

    ScalarT max_bevel_top =
        radius_top * std::tan(a2); // max bevel against other radius and inverse slope
    max_bevel_top = std::min(height * .5f, max_bevel_top); // also check against height

    return std::pair<ScalarT, ScalarT>{max_bevel_top, max_bevel_bottom};
}

} // namespace

struct RoundedConeConfig
{
    using Scalar = float;
    using Index = uint32_t;

    // Shape parameters.
    Scalar radius_top = 0;
    Scalar radius_bottom = 1;
    Scalar height = 1;
    Scalar bevel_radius_top = 0;
    Scalar bevel_radius_bottom = 0;
    Index num_radial_sections = 32;
    Index num_segments_top = 1;
    Index num_segments_bottom = 1;
    Index num_straight_segments = 1;
    Scalar start_sweep_angle = 0;
    Scalar end_sweep_angle = static_cast<Scalar>(2 * lagrange::internal::pi);
    Eigen::Matrix<Scalar, 3, 1> center{0, 0, 0};
    bool with_cross_section = true;

    // Cap parameters
    bool with_top_cap = true;
    bool with_bottom_cap = true;

    // Output parameters.
    bool output_normals = true;

    // Tolerances.
    /**
     * Two vertices are considered coinciding if the distance between them is
     * smaller than `dist_threshold`.
     */
    Scalar dist_threshold = static_cast<Scalar>(1e-6);

    /**
     * An edge is considered sharp if its dihedral angle is larger than
     * `angle_threshold`.
     */
    Scalar angle_threshold = static_cast<Scalar>(11 * lagrange::internal::pi / 180);

    /**
     * Numerical tolerence used for comparing Scalar values.
     */
    Scalar epsilon = static_cast<Scalar>(1e-6);

    void project_to_valid_range()
    {
        radius_top = std::max(radius_top, Scalar(0));
        radius_bottom = std::max(radius_bottom, Scalar(0));
        height = std::max(height, Scalar(0));
        std::pair<Scalar, Scalar> max_acceptable_bevels =
            get_max_cone_bevel(radius_top, radius_bottom, height);
        bevel_radius_top =
            std::min(std::max(bevel_radius_top, Scalar(0)), max_acceptable_bevels.first);
        bevel_radius_bottom =
            std::min(std::max(bevel_radius_bottom, Scalar(0)), max_acceptable_bevels.second);
        num_radial_sections = std::max(num_radial_sections, Index(1));
        num_segments_top = std::max(num_segments_top, Index(1));
        num_segments_bottom = std::max(num_segments_bottom, Index(1));
        num_straight_segments = std::max(num_straight_segments, Index(1));
    }
};

template <typename MeshType>
std::unique_ptr<MeshType> generate_rounded_cone(RoundedConeConfig config)
{
    using AttributeArray = typename MeshType::AttributeArray;
    using VertexArray = typename MeshType::VertexArray;
    using FacetArray = typename MeshType::FacetArray;
    using UVArray = typename MeshType::UVArray;
    using Index = typename MeshType::Index;
    using Scalar = typename MeshType::Scalar;
    using Point = Eigen::Matrix<Scalar, 1, 3>;
    using Generator_function = std::function<Point(Scalar)>;

    std::vector<std::shared_ptr<MeshType>> meshes;
    std::vector<GeometricProfile<VertexArray, Index>> profiles;

    config.project_to_valid_range();

    /*
     * Handle Empty Mesh
     */
    if (config.height < config.dist_threshold || (config.radius_top < config.dist_threshold &&
                                                  config.radius_bottom < config.dist_threshold)) {
        auto mesh = create_empty_mesh<VertexArray, FacetArray>();
        lagrange::set_uniform_semantic_label(*mesh, PrimitiveSemanticLabel::SIDE);
        return mesh;
    }

    /*
                /\
               /  \
              /    \
           r₁(______)r₁

    */

    /*
     * Generate Torus for base with radius = r₁ starting from 270 degrees
     */
    Scalar r1 = config.bevel_radius_bottom;
    Scalar theta = std::atan2(config.height, fabs(config.radius_bottom - config.radius_top));
    Scalar base_angle =
        (Scalar)(config.radius_bottom > config.radius_top ? 0.5 * theta
                                                          : 0.5 * (lagrange::internal::pi - theta));
    Scalar base_start_angle = (Scalar)(2 * lagrange::internal::pi * 3 / 4);
    Scalar base_reduction = (Scalar)(base_angle > 0.0 ? r1 / std::tan(base_angle) : 0.0);
    Scalar base_radius_post_bevel = config.radius_bottom - base_reduction;
    Scalar base_slice = (Scalar)(lagrange::internal::pi - 2 * base_angle);

    auto bottom_torus_generator = lagrange::partial_torus_generator<Scalar, Point>(
        base_radius_post_bevel,
        r1,
        Eigen::Matrix<Scalar, 3, 1>(0.0, r1, 0.0),
        base_start_angle,
        base_slice);
    auto bottom_torus_profile = generate_profile<MeshType, Point>(
        bottom_torus_generator,
        static_cast<Scalar>(config.num_segments_bottom));

    /*
     * Generate Torus for top with radius = r₂ ending at 90 degrees
     */
    Scalar r2 = config.bevel_radius_top;
    Scalar top_angle = (Scalar)(0.5 * (lagrange::internal::pi - 2 * base_angle));
    Scalar top_start_angle = (Scalar)(2 * top_angle - lagrange::internal::pi / 2);
    Scalar top_reduction = (Scalar)(top_angle > 0.0 ? r2 / std::tan(top_angle) : 0.0);
    Scalar top_radius_post_bevel = config.radius_top - top_reduction;
    Scalar top_slice = (Scalar)(lagrange::internal::pi - 2 * top_angle);

    // Generate Cone with bevel parameters
    Scalar base_height_offset = base_reduction * std::sin(2 * base_angle);
    Scalar top_height_offset = top_reduction * std::sin(2 * base_angle);
    Scalar cone_radius_bottom = config.radius_bottom - base_reduction * std::cos(2 * base_angle);
    Scalar cone_radius_top = config.radius_top + top_reduction * std::cos(2 * base_angle);

    auto top_torus_generator = lagrange::partial_torus_generator<Scalar, Point>(
        top_radius_post_bevel,
        r2,
        Eigen::Matrix<Scalar, 3, 1>(0.0, config.height - r2, 0.0),
        top_start_angle,
        top_slice);
    auto top_torus_profile = generate_profile<MeshType, Point>(
        top_torus_generator,
        safe_cast<Index>(config.num_segments_top));


    // Bottom of cone is origin. Cone is created around y axis
    Generator_function truncated_cone_generator = [&](Scalar t) -> Point {
        // Calculate the radius of the current row
        Point vert;
        Scalar diff = cone_radius_top - cone_radius_bottom;
        Scalar cone_radius = t * diff + cone_radius_bottom;
        vert << cone_radius, (config.height - top_height_offset) * t + (1 - t) * base_height_offset,
            0.0;
        return vert;
    };

    auto truncated_cone_profile = generate_profile<MeshType, Point>(
        truncated_cone_generator,
        safe_cast<Index>(config.num_straight_segments));

    // Stitch profiles for different parts top-down
    if (r1 > config.dist_threshold) {
        profiles.push_back(bottom_torus_profile);
    }

    profiles.push_back(truncated_cone_profile);

    if (r2 > config.dist_threshold) {
        profiles.push_back(top_torus_profile);
    }

    auto final_profile = lagrange::combine_geometric_profiles<VertexArray, Index>(profiles);

    Scalar sweep_angle = compute_sweep_angle(config.start_sweep_angle, config.end_sweep_angle);
    sweep_angle = std::min((Scalar)(2 * lagrange::internal::pi), sweep_angle);

    auto truncated_cone = lagrange::sweep<MeshType>(
        final_profile,
        safe_cast<Index>(config.num_radial_sections),
        config.radius_top,
        config.radius_bottom,
        r2,
        r1,
        top_slice,
        base_slice,
        config.start_sweep_angle,
        sweep_angle);

    // Disk is in x-z plane (z is negative in clockwise direction)
    auto top_cap = lagrange::generate_disk<MeshType>(
        top_radius_post_bevel,
        safe_cast<Index>(config.num_radial_sections),
        config.start_sweep_angle,
        sweep_angle,
        Eigen::Matrix<Scalar, 3, 1>(0.0, config.height, 0.0),
        false);
    auto bottom_cap = lagrange::generate_disk<MeshType>(
        base_radius_post_bevel,
        safe_cast<Index>(config.num_radial_sections),
        config.start_sweep_angle,
        sweep_angle);

    lagrange::set_uniform_semantic_label(*top_cap, PrimitiveSemanticLabel::TOP);
    lagrange::set_uniform_semantic_label(*truncated_cone, PrimitiveSemanticLabel::SIDE);
    lagrange::set_uniform_semantic_label(*bottom_cap, PrimitiveSemanticLabel::BOTTOM);

    // Avoid generating degenerate geometry
    if (config.height > config.dist_threshold) {
        meshes.push_back(std::move(truncated_cone));
    }

    if (config.radius_top > config.dist_threshold && config.with_top_cap) {
        meshes.push_back(std::move(top_cap));
    }

    if (config.radius_bottom > config.dist_threshold && config.with_bottom_cap) {
        meshes.push_back(std::move(bottom_cap));
    }

    // Again allow some tolerance when comparing to 2*lagrange::internal::pi
    if (config.with_cross_section && config.height > config.dist_threshold &&
        sweep_angle < 2 * lagrange::internal::pi - config.epsilon) {
        // Rotate profile by start angle for slicing
        auto rotated_profile = lagrange::rotate_geometric_profile(
            final_profile,
            static_cast<Scalar>(config.start_sweep_angle));

        // It's a slice, close cross sections of the mesh
        auto num_samples = rotated_profile.num_samples;
        auto end_profile = lagrange::rotate_geometric_profile(rotated_profile, sweep_angle);

        // Project cone profile on Y axis
        VertexArray center_samples(num_samples, 3);
        center_samples.block(0, 0, num_samples, 3) = rotated_profile.samples;
        center_samples.col(0).array() = 0.0;
        center_samples.col(2).array() = 0.0;

        GeometricProfile<VertexArray, Index> center_profile(center_samples, num_samples);
        std::vector<GeometricProfile<VertexArray, Index>> profiles_to_connect = {
            end_profile,
            center_profile,
            rotated_profile};
        auto cross_section =
            lagrange::connect_geometric_profiles_with_facets<MeshType>(profiles_to_connect);

        // UV mapping
        AttributeArray uvs(cross_section->get_num_vertices(), 2);
        auto uv_samples = final_profile.samples.leftCols(2);
        uvs.block(0, 0, num_samples, 2) = uv_samples;
        uvs.col(0).array() *= -1.0f;
        uvs.block(num_samples, 0, num_samples, 2) = center_samples.leftCols(2);
        uvs.block(2 * num_samples, 0, num_samples, 2) = uv_samples;

        const auto xmin = uvs.col(0).minCoeff();
        const auto ymin = uvs.col(1).minCoeff();

        // Recenter uv coordinates to begin from (0,0)
        if (xmin < 0) {
            uvs.col(0).array() += -1.0f * xmin;
        }
        if (ymin < 0) {
            uvs.col(1).array() += -1.0f * ymin;
        }
        cross_section->initialize_uv(uvs, cross_section->get_facets());

        lagrange::set_uniform_semantic_label(*cross_section, PrimitiveSemanticLabel::SIDE);
        meshes.push_back(std::move(cross_section));
    }
    // Combine all meshes
    auto mesh = lagrange::combine_mesh_list(meshes, true);

    // Zip boundary.
    {
        const auto bbox_diag =
            std::hypot(config.height, std::max(config.radius_top, config.radius_bottom));
        mesh = lagrange::bvh::zip_boundary(*mesh, 1e-6f * bbox_diag);
    }

    // Clean up mesh
    if ((config.radius_top > config.dist_threshold ||
         config.radius_bottom > config.dist_threshold) &&
        config.height > config.dist_threshold) {
        mesh = remove_degenerate_triangles(*mesh);
    }

    // Add corner normals
    if (config.output_normals) {
        if (config.radius_top == 0 && config.bevel_radius_top == 0 &&
            config.height > config.dist_threshold) {
            const auto& vertices = mesh->get_vertices();
            const auto max_y_value = vertices.col(1).maxCoeff();
            std::vector<Index> cone_vertices;
            for (auto vi : range(mesh->get_num_vertices())) {
                if (vertices(vi, 1) > max_y_value - config.dist_threshold) {
                    cone_vertices.push_back(vi);
                }
            }
            if (cone_vertices.size() != 1) {
                logger().warn(
                    "Generated cone has {} apexes.  Expecting only 1.",
                    cone_vertices.size());
            }
            compute_normal(*mesh, config.angle_threshold, cone_vertices);
        } else {
            compute_normal(*mesh, config.angle_threshold);
        }
        la_runtime_assert(mesh->has_indexed_attribute("normal"));
    }

    // Transform center
    {
        VertexArray vertices;
        mesh->export_vertices(vertices);
        vertices.col(1).array() -= config.height / 2;
        vertices.rowwise() += config.center.transpose().template cast<Scalar>();
        mesh->import_vertices(vertices);
    }

    // Normalize UVs
    if (mesh->is_uv_initialized()) {
        const auto uv_mesh = mesh->get_uv_mesh();
        UVArray uvs = uv_mesh->get_vertices();
        normalize_to_unit_box(uvs);
        mesh->initialize_uv(uvs, uv_mesh->get_facets());
    }

    packing::compute_rectangle_packing(*mesh);
    return mesh;
}

template <typename MeshType>
std::unique_ptr<MeshType> generate_rounded_cone(
    typename MeshType::Scalar radius_top,
    typename MeshType::Scalar radius_bottom,
    typename MeshType::Scalar height,
    typename MeshType::Scalar bevel_radius_top,
    typename MeshType::Scalar bevel_radius_bottom,
    typename MeshType::Index num_radial_sections,
    typename MeshType::Index num_segments_top,
    typename MeshType::Index num_segments_bottom,
    typename MeshType::Scalar start_sweep_angle = 0.0,
    typename MeshType::Scalar end_sweep_angle = 2 * lagrange::internal::pi,
    typename MeshType::Index num_straight_segments = 1,
    bool with_top_cap = true,
    bool with_bottom_cap = true)
{
    using Scalar = typename RoundedConeConfig::Scalar;
    using Index = typename RoundedConeConfig::Index;

    RoundedConeConfig config;
    config.radius_top = safe_cast<Scalar>(radius_top);
    config.radius_bottom = safe_cast<Scalar>(radius_bottom);
    config.height = safe_cast<Scalar>(height);
    config.bevel_radius_top = safe_cast<Scalar>(bevel_radius_top);
    config.bevel_radius_bottom = safe_cast<Scalar>(bevel_radius_bottom);
    config.num_radial_sections = safe_cast<Index>(num_radial_sections);
    config.num_segments_top = safe_cast<Index>(num_segments_top);
    config.num_segments_bottom = safe_cast<Index>(num_segments_bottom);
    config.start_sweep_angle = safe_cast<Scalar>(start_sweep_angle);
    config.end_sweep_angle = safe_cast<Scalar>(end_sweep_angle);
    config.num_straight_segments = safe_cast<Index>(num_straight_segments);
    config.with_top_cap = with_top_cap;
    config.with_bottom_cap = with_bottom_cap;

    return generate_rounded_cone<MeshType>(std::move(config));
}
} // namespace legacy
} // namespace primitive
} // namespace lagrange
