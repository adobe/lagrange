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
#include <vector>

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
#include <lagrange/utils/safe_cast.h>

namespace lagrange {
namespace primitive {
LAGRANGE_LEGACY_INLINE
namespace legacy {

struct RoundedCylinderConfig
{
    using Scalar = float;
    using Index = uint32_t;

    ///
    /// @name Shape parameters.
    /// @{
    Scalar radius = 1;
    Scalar height = 1;
    Scalar bevel_radius = 0;
    Index num_radial_sections = 64; ///< Number of sections used for top/bottom disc.
    Index num_bevel_segments = 1; ///< Number of isolines used for rounded bevel.
    Index num_straight_segments = 1; ///< Number of isolines on the cylinder side.
    Scalar start_sweep_angle = 0;
    Scalar end_sweep_angle = static_cast<Scalar>(2 * lagrange::internal::pi);
    Eigen::Matrix<Scalar, 3, 1> center{0, 0, 0};
    bool with_top_cap = true;
    bool with_bottom_cap = true;
    bool with_cross_section = true;

    /// @}
    /// @name Output parameters.
    /// @{
    bool output_normals = true;

    /// @}
    /// @name Tolerances.
    /// @{
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
    /// @}

    /**
     * Project config setting into valid range.
     *
     * This method ensure all lengths parameters are non-negative, and clip the
     * bevel radius parameter to its valid range.
     */
    void project_to_valid_range()
    {
        radius = std::max(radius, static_cast<Scalar>(0.0));
        height = std::max(height, static_cast<Scalar>(0.0));
        bevel_radius = std::min(
            std::max(bevel_radius, static_cast<Scalar>(0.0)),
            std::min(radius, height * static_cast<Scalar>(0.5)));
        num_radial_sections = std::max(num_radial_sections, static_cast<Index>(1));
        num_bevel_segments = std::max(num_bevel_segments, static_cast<Index>(1));
        num_straight_segments = std::max(num_straight_segments, static_cast<Index>(1));
    }
};

/**
 * Generate rounded cylinder.  The cylinder axis is parallel to the Y axis.
 *
 * @param[in] config  Configuration parameters.
 *
 * @returns The genrated mesh.  UVs and normals are initialized based on the
 *          config settings.
 */
template <typename MeshType>
std::unique_ptr<MeshType> generate_rounded_cylinder(RoundedCylinderConfig config)
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
    if (config.height < config.dist_threshold || config.radius < config.dist_threshold) {
        auto mesh = create_empty_mesh<VertexArray, FacetArray>();
        lagrange::set_uniform_semantic_label(*mesh, PrimitiveSemanticLabel::SIDE);
        return mesh;
    }

    /*
            r(¯¯¯¯¯¯)r
             |      |
             |      |
             |      |
             |      |
            r(______)r, theta = pi/2

    */


    Scalar r = config.bevel_radius;
    Scalar torus_start_angle = 2 * lagrange::internal::pi * 3 / 4;
    Scalar radius_post_bevel = config.radius - r;
    Scalar torus_slice = (lagrange::internal::pi / 2);
    Scalar sweep_angle = compute_sweep_angle(config.start_sweep_angle, config.end_sweep_angle);
    // take into account numerical errors
    sweep_angle = std::min<Scalar>(2 * lagrange::internal::pi, sweep_angle);

    /*
     *Generate Torus for base with radius = r starting from 270 degrees
     */
    auto bottom_torus_generator = lagrange::partial_torus_generator<Scalar, Point>(
        radius_post_bevel,
        r,
        Eigen::Matrix<Scalar, 3, 1>(0.0, r, 0.0),
        torus_start_angle,
        torus_slice);
    auto bottom_torus_profile = generate_profile<MeshType, Point>(
        bottom_torus_generator,
        safe_cast<Index>(config.num_bevel_segments));

    /*
     *Generate Torus for top with radius = r starting from 0 degrees
     */
    auto top_torus_generator = lagrange::partial_torus_generator<Scalar, Point>(
        radius_post_bevel,
        r,
        Eigen::Matrix<Scalar, 3, 1>(0.0, config.height - r, 0.0),
        0.0,
        torus_slice);
    auto top_torus_profile = generate_profile<MeshType, Point>(
        top_torus_generator,
        safe_cast<Index>(config.num_bevel_segments));

    // Bottom of cylinder is origin. Cylinder is created around y axis
    Generator_function cylinder_generator = [&](Scalar t) -> Point {
        Point vert;
        vert << config.radius, (config.height - 2 * r) * t + r, 0.0;
        return vert;
    };

    auto cylinder_profile = generate_profile<MeshType, Point>(
        cylinder_generator,
        safe_cast<Index>(config.num_straight_segments));

    // Stitch profiles for different parts top-down
    if (r > config.dist_threshold) {
        profiles.push_back(bottom_torus_profile);
    }

    if (config.height > 2 * r + config.dist_threshold) {
        profiles.push_back(cylinder_profile);
    }

    if (r > config.dist_threshold) {
        profiles.push_back(top_torus_profile);
    }

    auto final_profile = lagrange::combine_geometric_profiles<VertexArray, Index>(profiles);
    auto cylinder = lagrange::sweep<MeshType>(
        final_profile,
        safe_cast<Index>(config.num_radial_sections),
        config.radius,
        config.radius,
        r,
        r,
        torus_slice,
        torus_slice,
        config.start_sweep_angle,
        sweep_angle);

    // Disk is in x-z plane (z is negative in clockwise direction)
    auto top_cap = lagrange::generate_disk<MeshType>(
        radius_post_bevel,
        safe_cast<Index>(config.num_radial_sections),
        config.start_sweep_angle,
        sweep_angle,
        Eigen::Matrix<Scalar, 3, 1>(0.0, config.height, 0.0),
        false);
    auto bottom_cap = lagrange::generate_disk<MeshType>(
        radius_post_bevel,
        safe_cast<Index>(config.num_radial_sections),
        config.start_sweep_angle,
        sweep_angle);

    lagrange::set_uniform_semantic_label(*top_cap, PrimitiveSemanticLabel::TOP);
    lagrange::set_uniform_semantic_label(*cylinder, PrimitiveSemanticLabel::SIDE);
    lagrange::set_uniform_semantic_label(*bottom_cap, PrimitiveSemanticLabel::BOTTOM);

    // Avoid generating degenerate geometry
    if (config.height > config.dist_threshold) {
        meshes.push_back(std::move(cylinder));
    }

    if (radius_post_bevel > config.dist_threshold) {
        if (config.with_top_cap) {
            meshes.push_back(std::move(top_cap));
        }
        if (config.with_bottom_cap) {
            meshes.push_back(std::move(bottom_cap));
        }
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

        // Project cylinder profile on Y axis
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
        uvs.col(0).array() *= -1.0;
        uvs.block(num_samples, 0, num_samples, 2) = center_samples.leftCols(2);
        uvs.block(2 * num_samples, 0, num_samples, 2) = uv_samples;

        const auto xmin = uvs.col(0).minCoeff();
        const auto ymin = uvs.col(1).minCoeff();

        // Recenter uv coordinates to begin from (0,0)
        if (xmin < 0.0) {
            uvs.col(0).array() += -1.0 * xmin;
        }
        if (ymin < 0.0) {
            uvs.col(1).array() += -1.0 * ymin;
        }
        cross_section->initialize_uv(uvs, cross_section->get_facets());

        lagrange::set_uniform_semantic_label(*cross_section, PrimitiveSemanticLabel::SIDE);
        meshes.push_back(std::move(cross_section));
    }

    // Combine all meshes
    auto mesh = lagrange::combine_mesh_list(meshes, true);
    {
        const auto bbox_diag = std::hypot(config.height, config.radius * 2);
        mesh = lagrange::bvh::zip_boundary(*mesh, 1e-6 * bbox_diag);
    }

    // Clean up mesh
    if (config.radius > config.dist_threshold && config.height > config.dist_threshold) {
        mesh = remove_degenerate_triangles(*mesh);
    }

    // Add normals
    if (config.output_normals) {
        compute_normal(*mesh, config.angle_threshold);
        la_runtime_assert(mesh->has_indexed_attribute("normal"));
    }

    // Transform center
    {
        typename MeshType::VertexArray vertices;
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
std::unique_ptr<MeshType> generate_rounded_cylinder(
    typename MeshType::Scalar radius,
    typename MeshType::Scalar height,
    typename MeshType::Scalar bevel_radius,
    typename MeshType::Index num_radial_sections,
    typename MeshType::Index num_bevel_segments,
    typename MeshType::Scalar start_sweep_angle = 0.0,
    typename MeshType::Scalar end_sweep_angle = 2 * lagrange::internal::pi,
    typename MeshType::Index num_straight_segments = 1,
    bool with_top_cap = true,
    bool with_bottom_cap = true)
{
    using Scalar = typename RoundedCylinderConfig::Scalar;
    using Index = typename RoundedCylinderConfig::Index;

    RoundedCylinderConfig config;
    config.radius = safe_cast<Scalar>(radius);
    config.height = safe_cast<Scalar>(height);
    config.bevel_radius = safe_cast<Scalar>(bevel_radius);
    config.num_radial_sections = safe_cast<Index>(num_radial_sections);
    config.num_bevel_segments = safe_cast<Index>(num_bevel_segments);
    config.start_sweep_angle = safe_cast<Scalar>(start_sweep_angle);
    config.end_sweep_angle = safe_cast<Scalar>(end_sweep_angle);
    config.num_straight_segments = safe_cast<Index>(num_straight_segments);
    config.with_top_cap = with_top_cap;
    config.with_bottom_cap = with_bottom_cap;

    return generate_rounded_cylinder<MeshType>(std::move(config));
}
} // namespace legacy
} // namespace primitive
} // namespace lagrange
