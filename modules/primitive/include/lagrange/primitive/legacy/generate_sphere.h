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
#include <lagrange/bvh/zip_boundary.h>
#include <lagrange/compute_normal.h>
#include <lagrange/create_mesh.h>
#include <lagrange/internal/constants.h>
#include <lagrange/legacy/inline.h>
#include <lagrange/packing/compute_rectangle_packing.h>
#include <lagrange/primitive/generation_utils.h>
#include <lagrange/utils/safe_cast.h>
#include <lagrange/utils/warning.h>

namespace lagrange {
namespace primitive {
LAGRANGE_LEGACY_INLINE
namespace legacy {

LA_IGNORE_DEPRECATION_WARNING_BEGIN

struct SphereConfig
{
    using Scalar = float;
    using Index = uint32_t;

    ///
    /// @name Shape parameters.
    /// @{
    Scalar radius = 1;

    /**
     * @deprecated Use `start_sweep_angle` and `end_sweep_angle` instead.
     * This parameter will be removed in a future version.
     */
    [[deprecated]] Scalar sweep_angle = static_cast<Scalar>(2 * lagrange::internal::pi);
    Scalar start_sweep_angle = 0;
    Scalar end_sweep_angle = static_cast<Scalar>(2 * lagrange::internal::pi);
    Index num_longitude_sections = 32;
    Index num_latitude_sections = 32;
    Eigen::Matrix<Scalar, 3, 1> center{0, 0, 0};
    bool with_cross_section = true;

    /// @}
    /// @name Output parameters
    /// @{
    bool output_normals = true;

    /// @}
    /// @name Tolerances.
    /// @{
    /**
     * An edge is considered sharp if its dihedral angle is larger than
     * `angle_threshold`.
     */
    Scalar angle_threshold = static_cast<Scalar>(11 * lagrange::internal::pi / 180);

    /**
     * Numerical tolerence used for comparing Scalar values.
     */
    Scalar epsilon = static_cast<Scalar>(1e-6);
    ///@}

    /**
     * Project config setting into valid range.
     *
     * This method ensure all lengths parameters are non-negative, and clip the
     * radius parameter to its valid range.
     */
    void project_to_valid_range()
    {
        radius = std::max(radius, static_cast<Scalar>(0.0));
        num_longitude_sections = std::max(num_longitude_sections, static_cast<Index>(3));
        num_latitude_sections = std::max(num_latitude_sections, static_cast<Index>(3));

        LA_IGNORE_DEPRECATION_WARNING_BEGIN
        // If sweep_angle is set but start/end angles are default, update them
        if (sweep_angle != static_cast<Scalar>(2 * lagrange::internal::pi) &&
            start_sweep_angle == 0 &&
            end_sweep_angle == static_cast<Scalar>(2 * lagrange::internal::pi)) {
            end_sweep_angle = start_sweep_angle + sweep_angle;
        }
        LA_IGNORE_DEPRECATION_WARNING_END
    }

    /**
     * Get the effective sweep angle based on start and end angles.
     * This is used internally to maintain backward compatibility.
     */
    Scalar get_effective_sweep_angle() const { return end_sweep_angle - start_sweep_angle; }
};

template <typename MeshType>
std::unique_ptr<MeshType> generate_sphere(SphereConfig config)
{
    using Scalar = typename MeshType::Scalar;
    using Index = typename MeshType::Index;
    using VertexArray = typename MeshType::VertexArray;
    using FacetArray = typename MeshType::FacetArray;
    using AttributeArray = typename MeshType::AttributeArray;
    using UVArray = typename MeshType::UVArray;
    using UVIndices = typename MeshType::UVIndices;

    config.project_to_valid_range();

    // Use get_effective_sweep_angle() instead of sweep_angle directly
    bool is_closed =
        config.get_effective_sweep_angle() >= (2 * lagrange::internal::pi - config.epsilon);

    const Index repeat_x = safe_cast<Index>(config.num_longitude_sections);
    const Index repeat_y = safe_cast<Index>(config.num_latitude_sections);
    const Index row_size = is_closed ? repeat_x : (repeat_x + 1);
    const Index num_vertices = row_size * (repeat_y - 1) + 2 + (is_closed ? 0 : 1);
    const Index num_uvs = (repeat_x + 1) * (repeat_y + 1) - 2 +
                          (is_closed || !config.with_cross_section ? 0 : 2 * (repeat_y + 2));
    const Index num_triangles = repeat_x * (repeat_y - 1) * 2 +
                                (is_closed || !config.with_cross_section ? 0 : repeat_y * 2);

    VertexArray vertices(num_vertices, 3);
    FacetArray facets(num_triangles, 3);
    UVArray uvs(num_uvs, 2);
    UVIndices uv_indices(num_triangles, 3);
    AttributeArray normals(num_vertices, 3);

    auto get_vertex_index = [&](Index rx, Index ry) {
        Index vidx(0);
        if (ry == 0) {
            vidx = 0; // North pole.
        } else if (ry == repeat_y) {
            vidx = 1; // South pole.
        } else {
            vidx = 2 + row_size * (ry - 1) + (is_closed ? (rx % repeat_x) : rx);
        }
        return vidx;
    };

    auto get_uv_vertex_index = [&](Index rx, Index ry) {
        // The UV coordines of a sphere forms a rectangular grid.  However, due
        // to triangulation, the top left and bottom right corner are not used
        // (marked with "o").
        //
        //     o   +   +   +   <--- ry == repeat_y
        //       / | / | / |
        //     +---+---+---+
        //     | / | / | / |
        //     +---+---+---+
        //     | / | / | / |
        //     +---+---+---+
        //     | / | / | /
        //     +   +   +   o   <--- ry == 0
        //
        // Thus, an adjustment to the index is used to skip those unused grid
        // points.  Because Index type can be unsigned, we opt to compute the
        // positive adjutment and substract it from the result.
        const Index adjustment = ry > 0 ? (ry == repeat_y && rx > 0 ? 2 : 1) : 0;
        return ry * (repeat_x + 1) + rx - adjustment;
    };

    Scalar v_max = 1;
    Scalar u_max = (config.get_effective_sweep_angle() / lagrange::internal::pi) * v_max;
    for (auto ry : range(repeat_y + 1)) {
        for (auto rx : range(repeat_x + 1)) {
            const Scalar theta =
                ((Scalar)rx / (Scalar)repeat_x) * config.get_effective_sweep_angle() +
                config.start_sweep_angle;
            const Scalar phi = ((Scalar)ry / (Scalar)repeat_y) * lagrange::internal::pi;
            const Scalar x = std::cos(theta) * std::sin(phi);
            const Scalar y = std::cos(phi);
            const Scalar z = std::sin(theta) * std::sin(phi);

            const Index vertex_index = get_vertex_index(rx, ry);

            // vertices
            vertices.row(vertex_index) << config.radius * x + config.center.x(),
                config.radius * y + config.center.y(), config.radius * z + config.center.z();
            // normals
            normals.row(vertex_index) << x, y, z;

            // uvs
            const Index uv_index = get_uv_vertex_index(rx, ry);
            uvs(uv_index, 0) = (Scalar)u_max * (Scalar)(repeat_x - rx) / (Scalar)repeat_x;
            uvs(uv_index, 1) = (Scalar)v_max * (Scalar)(repeat_y - ry) / (Scalar)repeat_y;
        }
    }

    Index triangle_id = 0;
    for (auto ry : range(repeat_y)) {
        for (auto rx : range(repeat_x)) {
            const auto q0 = get_vertex_index(rx, ry);
            const auto q1 = get_vertex_index(rx + 1, ry);
            const auto q2 = get_vertex_index(rx + 1, ry + 1);
            const auto q3 = get_vertex_index(rx, ry + 1);

            const auto p0 = get_uv_vertex_index(rx, ry);
            const auto p1 = get_uv_vertex_index(rx + 1, ry);
            const auto p2 = get_uv_vertex_index(rx + 1, ry + 1);
            const auto p3 = get_uv_vertex_index(rx, ry + 1);

            if (q0 != q1) {
                facets.row(triangle_id) << q0, q1, q2;
                uv_indices.row(triangle_id) << p0, p1, p2;
                triangle_id++;
            }
            if (q2 != q3) {
                facets.row(triangle_id) << q0, q2, q3;
                uv_indices.row(triangle_id) << p0, p2, p3;
                triangle_id++;
            }
        }
    }

    if (!is_closed && config.with_cross_section) {
        Index center_index = static_cast<Index>(vertices.rows() - 1);
        vertices.row(center_index) = config.center.template cast<Scalar>();
        Index base_uv_index = (repeat_x + 1) * (repeat_y + 1) - 2;

        uvs.row(base_uv_index) << 0, 0;
        for (auto ry : range(repeat_y + 1)) {
            const Scalar phi = ((Scalar)ry / (Scalar)repeat_y) * lagrange::internal::pi;
            uvs.row(base_uv_index + ry + 1) << std::cos(phi) / lagrange::internal::pi,
                std::sin(phi) / lagrange::internal::pi;

            if (ry != repeat_y) {
                Index v0 = get_vertex_index(0, ry);
                Index v1 = get_vertex_index(0, ry + 1);
                facets.row(triangle_id) << v0, v1, center_index;
                uv_indices.row(triangle_id) << base_uv_index + ry + 1, base_uv_index + ry + 2,
                    base_uv_index;
                triangle_id++;
            }
        }

        base_uv_index += repeat_y + 2;
        uvs.row(base_uv_index) << 0, 0;
        for (auto ry : range(repeat_y + 1)) {
            const Scalar phi = -((Scalar)ry / (Scalar)repeat_y) * lagrange::internal::pi;
            uvs.row(base_uv_index + ry + 1) << std::cos(phi) / lagrange::internal::pi,
                std::sin(phi) / lagrange::internal::pi;

            if (ry != repeat_y) {
                Index v0 = get_vertex_index(repeat_x, ry);
                Index v1 = get_vertex_index(repeat_x, ry + 1);
                facets.row(triangle_id) << v1, v0, center_index;
                uv_indices.row(triangle_id) << base_uv_index + ry + 2, base_uv_index + ry + 1,
                    base_uv_index;
                triangle_id++;
            }
        }
    }

    assert(triangle_id == num_triangles);

    auto mesh = lagrange::create_mesh(vertices, facets);

    mesh->initialize_uv(uvs, uv_indices);

    if (config.output_normals) {
        compute_normal(*mesh, config.angle_threshold);
        la_runtime_assert(mesh->has_indexed_attribute("normal"));
    }

    // Set uniform semantic label
    lagrange::set_uniform_semantic_label(*mesh, PrimitiveSemanticLabel::TOP);

    packing::compute_rectangle_packing(*mesh);
    return mesh;
}

template <typename MeshType>
std::unique_ptr<MeshType> generate_sphere(
    typename MeshType::Scalar radius,
    const Eigen::Matrix<typename MeshType::Scalar, 3, 1>& center = {0, 0, 0},
    typename MeshType::Scalar sweep_angle = 2 * lagrange::internal::pi,
    typename MeshType::Index num_radial_sections = 50,
    typename MeshType::Index flat_shade_thresh = 20)
{
    using Scalar = typename SphereConfig::Scalar;
    using Index = typename SphereConfig::Index;

    SphereConfig config;
    config.radius = safe_cast<Scalar>(radius);
    config.center = center.template cast<Scalar>();
    config.sweep_angle = safe_cast<Scalar>(sweep_angle);
    config.end_sweep_angle = config.start_sweep_angle + config.sweep_angle;
    config.num_longitude_sections = safe_cast<Index>(num_radial_sections);
    config.num_latitude_sections = safe_cast<Index>(num_radial_sections);
    config.angle_threshold =
        safe_cast<Scalar>((num_radial_sections < flat_shade_thresh) ? 0 : config.angle_threshold);

    return generate_sphere<MeshType>(std::move(config));
}

LA_IGNORE_DEPRECATION_WARNING_END

} // namespace legacy
} // namespace primitive
} // namespace lagrange
