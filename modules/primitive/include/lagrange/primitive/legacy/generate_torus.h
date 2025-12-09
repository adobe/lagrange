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

#include <lagrange/Mesh.h>
#include <lagrange/attributes/attribute_utils.h>
#include <lagrange/bvh/zip_boundary.h>
#include <lagrange/combine_mesh_list.h>
#include <lagrange/compute_normal.h>
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

struct TorusConfig
{
    using Scalar = float;
    using Index = uint32_t;

    ///
    /// @name Shape parameters.
    /// @{
    Scalar major_radius = 5;
    Scalar minor_radius = 1;
    Index ring_segments = 50;
    Index pipe_segments = 50;
    Eigen::Matrix<Scalar, 3, 1> center{0, 0, 0};
    Scalar start_sweep_angle = 0;
    Scalar end_sweep_angle = static_cast<Scalar>(2 * lagrange::internal::pi);
    bool with_caps = true;

    /// @}
    /// @name Output parameters.
    /// @{
    bool output_normals = true;

    /// @}
    /// @name Tolerances.
    /// @{
    /**
     * Two vertices are considered coinciding iff the distance between them is
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
     * radius parameter to its valid range.
     */
    void project_to_valid_range()
    {
        minor_radius = std::max(minor_radius, static_cast<Scalar>(0.0));
        major_radius = std::max(major_radius, static_cast<Scalar>(minor_radius));
        ring_segments = std::max(ring_segments, static_cast<Index>(3));
        pipe_segments = std::max(pipe_segments, static_cast<Index>(3));
    }
};

template <typename MeshType>
std::unique_ptr<MeshType> generate_torus(TorusConfig config)
{
    using AttributeArray = typename MeshType::AttributeArray;
    using VertexArray = typename MeshType::VertexArray;
    using FacetArray = typename MeshType::FacetArray;
    using UVArray = typename MeshType::UVArray;
    using Scalar = typename MeshType::Scalar;
    using Index = typename MeshType::Index;
    using Point = Eigen::Matrix<Scalar, 1, 3>;
    using Generator_function = std::function<Point(Scalar)>;

    config.project_to_valid_range();

    /*
     * Handle Empty Mesh
     */
    if (config.major_radius < config.dist_threshold) {
        auto mesh = create_empty_mesh<VertexArray, FacetArray>();
        lagrange::set_uniform_semantic_label(*mesh, PrimitiveSemanticLabel::SIDE);
        return mesh;
    }

    std::vector<std::shared_ptr<MeshType>> meshes;
    // Init defaults
    Scalar torus_start_angle = lagrange::internal::pi; // Inner UV Seam
    Scalar torus_slice = 2 * lagrange::internal::pi;
    Scalar sweep_angle = compute_sweep_angle(config.start_sweep_angle, config.end_sweep_angle);
    // take into account numerical errors
    sweep_angle = std::min<Scalar>(2 * lagrange::internal::pi, sweep_angle);

    /**
     * Returns a lambda function to generate a torus with input parameters
     * starting at start_slice_angle such that the angle subtended by each span=t is slice_angle.
     */
    Generator_function torus_generator = lagrange::partial_torus_generator<Scalar, Point>(
        config.major_radius,
        config.minor_radius,
        config.center.template cast<Scalar>(),
        torus_start_angle,
        torus_slice);
    auto torus_profile = generate_profile<MeshType, Point>(torus_generator, config.pipe_segments);
    auto torus_mesh = lagrange::sweep<MeshType>(
        {torus_profile},
        safe_cast<Index>(config.ring_segments),
        config.major_radius,
        config.major_radius,
        0.0,
        0.0,
        torus_slice,
        torus_slice,
        config.start_sweep_angle,
        sweep_angle);

    meshes.push_back(std::move(torus_mesh));


    // Again allow some tolerance when comparing to 2*lagrange::internal::pi
    if (config.with_caps && config.major_radius > config.dist_threshold &&
        sweep_angle < 2 * lagrange::internal::pi - config.epsilon) {
        using Vector3S = Eigen::Matrix<Scalar, 3, 1>;
        // Rotate profile by start angle for slicing
        auto rotated_profile = lagrange::rotate_geometric_profile(
            torus_profile,
            static_cast<Scalar>(config.start_sweep_angle));
        auto end_profile = lagrange::rotate_geometric_profile(rotated_profile, sweep_angle);
        auto rotation_start = Eigen::AngleAxis<Scalar>(config.start_sweep_angle, Vector3S::UnitY());
        auto rotation_subtended = Eigen::AngleAxis<Scalar>(sweep_angle, Vector3S::UnitY());

        auto start_sample = torus_profile.samples.row(0);
        auto cross_section_center =
            Vector3S(start_sample[0] + config.minor_radius, start_sample[1], start_sample[2]);
        auto center_start = rotation_start * cross_section_center;
        auto center_end = rotation_subtended * center_start;

        auto cross_section_start =
            lagrange::fan_triangulate_profile<MeshType>(rotated_profile, center_start, true);
        auto cross_section_end =
            lagrange::fan_triangulate_profile<MeshType>(end_profile, center_end);

        // UV mapping
        AttributeArray uvs(cross_section_start->get_num_vertices(), 2);
        auto uv_samples = torus_profile.samples.leftCols(2);
        uvs.row(0) << cross_section_center[0], cross_section_center[1];
        uvs.block(1, 0, torus_profile.num_samples, 2) = uv_samples;

        const auto xmin = uvs.col(0).minCoeff();
        const auto ymin = uvs.col(1).minCoeff();

        // Recenter uv coordinates to begin from (0,0)
        if (xmin < 0.0) {
            uvs.col(0).array() += -1.0 * xmin;
        }
        if (ymin < 0.0) {
            uvs.col(1).array() += -1.0 * ymin;
        }

        cross_section_start->initialize_uv(uvs, cross_section_start->get_facets());
        cross_section_end->initialize_uv(uvs, cross_section_end->get_facets());

        meshes.push_back(std::move(cross_section_start));
        meshes.push_back(std::move(cross_section_end));
    }

    // Combine all meshes
    auto mesh = lagrange::combine_mesh_list(meshes, true);
    const auto& vertices = mesh->get_vertices();
    const auto bbox_diag = (vertices.colwise().maxCoeff() - vertices.colwise().minCoeff()).norm();

    mesh = lagrange::bvh::zip_boundary(
        *mesh,
        std::min(
            static_cast<Scalar>(1e-6 * bbox_diag),
            static_cast<Scalar>(config.dist_threshold)));

    // Add normals
    if (config.output_normals) {
        compute_normal(*mesh, config.angle_threshold);
        la_runtime_assert(mesh->has_indexed_attribute("normal"));
    }

    // Normalize UVs
    if (mesh->is_uv_initialized()) {
        const auto uv_mesh = mesh->get_uv_mesh();
        UVArray uvs = uv_mesh->get_vertices();
        normalize_to_unit_box(uvs);
        mesh->initialize_uv(uvs, uv_mesh->get_facets());
    }

    // Clean up mesh
    mesh = remove_degenerate_triangles(*mesh);

    // Set uniform semantic label
    lagrange::set_uniform_semantic_label(*mesh, PrimitiveSemanticLabel::TOP);

    packing::compute_rectangle_packing(*mesh);
    return mesh;
}

template <typename MeshType>
std::unique_ptr<MeshType> generate_torus(
    typename MeshType::Scalar major_radius,
    typename MeshType::Scalar minor_radius,
    typename MeshType::Index ring_segments = 50,
    typename MeshType::Index pipe_segments = 50,
    Eigen::Matrix<typename MeshType::Scalar, 3, 1> center =
        Eigen::Matrix<typename MeshType::Scalar, 3, 1>(0.0, 0.0, 0.0),
    typename MeshType::Scalar start_sweep_angle = 0.0,
    typename MeshType::Scalar end_sweep_angle = 2 * lagrange::internal::pi)
{
    using Scalar = typename TorusConfig::Scalar;
    using Index = typename TorusConfig::Index;

    TorusConfig config;
    config.major_radius = safe_cast<Scalar>(major_radius);
    config.minor_radius = safe_cast<Scalar>(minor_radius);
    config.ring_segments = safe_cast<Index>(ring_segments);
    config.pipe_segments = safe_cast<Index>(pipe_segments);
    config.center = center.template cast<Scalar>();
    config.start_sweep_angle = safe_cast<Scalar>(start_sweep_angle);
    config.end_sweep_angle = safe_cast<Scalar>(end_sweep_angle);

    return generate_torus<MeshType>(std::move(config));
}
} // namespace legacy
} // namespace primitive
} // namespace lagrange
