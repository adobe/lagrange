/*
 * Copyright 2026 Adobe. All rights reserved.
 * This file is licensed to you under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License. You may obtain a copy
 * of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under
 * the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
 * OF ANY KIND, either express or implied. See the License for the specific language
 * governing permissions and limitations under the License.
 */

#include <lagrange/raycasting/compute_local_feature_size.h>

#include "closest_vertex_from_barycentric.h"
#include "prepare_ray_caster.h"

#include <lagrange/Attribute.h>
#include <lagrange/SurfaceMeshTypes.h>
#include <lagrange/cast_attribute.h>
#include <lagrange/compute_vertex_normal.h>
#include <lagrange/internal/constants.h>
#include <lagrange/internal/find_attribute_utils.h>
#include <lagrange/raycasting/RayCaster.h>
#include <lagrange/utils/Error.h>
#include <lagrange/utils/StackSet.h>
#include <lagrange/utils/assert.h>
#include <lagrange/views.h>

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <tbb/parallel_for.h>
#include <Eigen/Core>
#include <lagrange/utils/warnon.h>
// clang-format on

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

namespace lagrange::raycasting {

namespace {

// Maximum number of vertices in a one-ring neighborhood (stack-allocated).
constexpr size_t kMaxOneRingSize = 64;

// Collect the one-ring vertex neighborhood of a vertex into a StackSet.
template <typename Scalar, typename Index>
StackSet<Index, kMaxOneRingSize> collect_one_ring(
    const SurfaceMesh<Scalar, Index>& mesh,
    Index vertex_index)
{
    StackSet<Index, kMaxOneRingSize> one_ring;
    one_ring.insert(vertex_index);
    mesh.foreach_edge_around_vertex_with_duplicates(vertex_index, [&](Index edge_id) {
        auto v0 = mesh.get_edge_vertices(edge_id);
        Index neighbor = (v0[0] == vertex_index) ? v0[1] : v0[0];
        one_ring.insert(neighbor);
    });
    return one_ring;
}

// Binary search for the medial axis point along a ray direction.
// Returns the maximum distance along the ray where the closest point on the surface
// still corresponds to the same vertex (vertex_index).
// If search fails or no valid medial axis point exists, returns max_depth.
template <typename Index, typename FacetView>
float find_medial_axis_distance(
    const RayCaster* ray_caster,
    const StackSet<Index, kMaxOneRingSize>& one_ring,
    const FacetView& facets,
    const Eigen::Vector3f& start_point,
    const Eigen::Vector3f& direction,
    float max_depth,
    float tolerance)
{
    // Binary search in range [0, max_depth]
    float min_d = 0.0f;
    float max_d = max_depth;

    while (max_d - min_d > tolerance) {
        float mid_d = (min_d + max_d) * 0.5f;

        // Query point at this depth
        Eigen::Vector3f query_point = start_point + mid_d * direction;

        // Find closest point on surface
        auto hit = ray_caster->closest_point(query_point);
        la_runtime_assert(
            hit.has_value(),
            "Ray caster should always return a hit for closed meshes");

        // Get the closest vertex of the hit triangle
        int local_vertex_idx =
            closest_vertex_from_barycentric(hit->barycentric_coord[0], hit->barycentric_coord[1]);

        // Get the global vertex index
        auto facet = facets.row(static_cast<Index>(hit->facet_index));
        Index closest_vertex = facet[local_vertex_idx];

        // Check if the closest vertex is within one ring of the original vertex
        if (one_ring.contains(closest_vertex)) {
            min_d = mid_d;
        } else {
            max_d = mid_d;
        }
    }

    // Return the maximum depth where we're still closest to the original vertex
    return min_d;
}

} // namespace

template <typename Scalar, typename Index>
AttributeId compute_local_feature_size(
    SurfaceMesh<Scalar, Index>& mesh,
    const LocalFeatureSizeOptions& options,
    const RayCaster* ray_caster)
{
    mesh.initialize_edges();
    la_runtime_assert(mesh.is_triangle_mesh(), "Only triangle meshes are supported");
    la_runtime_assert(mesh.get_dimension() == 3, "Only 3D meshes are supported");

    const Index num_vertices = mesh.get_num_vertices();

    // Create output attribute
    AttributeId lfs_id = internal::find_or_create_attribute<Scalar>(
        mesh,
        options.output_attribute_name,
        Vertex,
        AttributeUsage::Scalar,
        1,
        internal::ResetToDefault::Yes);

    if (num_vertices == 0) {
        // Nothing to do for empty mesh
        logger().warn("Mesh has no vertices. Returning empty local feature size.");
        return lfs_id;
    }

    auto lfs_values = mesh.template ref_attribute<Scalar>(lfs_id).ref_all();

    // Initialize with default_lfs (or infinity if no upper bound is set)
    std::fill(lfs_values.begin(), lfs_values.end(), static_cast<Scalar>(options.default_lfs));

    // Build a temporary ray caster if one is not provided
    auto engine = prepare_ray_caster(mesh, ray_caster);
    if (engine) {
        ray_caster = engine.get();
    }

    auto vertices = vertex_view(mesh);

    // Compute bounding box diagonal for scaling tolerance
    Eigen::Vector3f bbox_min = vertices.colwise().minCoeff().template cast<float>();
    Eigen::Vector3f bbox_max = vertices.colwise().maxCoeff().template cast<float>();
    float bbox_diagonal = (bbox_max - bbox_min).norm();

    // Scale relative parameters by bounding box diagonal
    float absolute_tolerance = options.medial_axis_tolerance * bbox_diagonal;
    float absolute_ray_offset = options.ray_offset * bbox_diagonal;
    logger().debug("Absolute tolerance: {}", absolute_tolerance);

    // Precompute vertex normals for all vertices
    AttributeId normal_id = invalid<AttributeId>();
    bool owns_normal_attribute = false;
    if (options.vertex_normal_attribute_name.empty()) {
        VertexNormalOptions normal_options;
        normal_options.output_attribute_name = "@vertex_normal_lfs_tmp";
        normal_options.weight_type = NormalWeightingType::Angle;
        normal_id = compute_vertex_normal(mesh, normal_options);
        owns_normal_attribute = true;
    } else {
        la_runtime_assert(
            mesh.has_attribute(options.vertex_normal_attribute_name),
            "Specified vertex normal attribute does not exist");
        if (!mesh.template is_attribute_type<Scalar>(options.vertex_normal_attribute_name)) {
            logger().info(
                "Casting vertex normal attribute {} to Scalar type",
                options.vertex_normal_attribute_name);
            normal_id = cast_attribute<Scalar>(
                mesh,
                options.vertex_normal_attribute_name,
                "@vertex_normal_lfs_tmp");
            owns_normal_attribute = true;
        } else {
            normal_id = mesh.get_attribute_id(options.vertex_normal_attribute_name);
        }
    }
    auto vertex_normals = attribute_matrix_view<Scalar>(mesh, normal_id);
    auto facets = facet_view(mesh);

    // Process each vertex
    tbb::parallel_for(Index(0), num_vertices, [&](Index vi) {
        Eigen::Vector3f vertex_pos = vertices.row(vi).template cast<float>().transpose();

        // Get precomputed vertex normal
        Eigen::Vector3f vertex_normal = vertex_normals.row(vi).template cast<float>().transpose();

        float lfs = options.default_lfs;

        // Precompute one-ring neighborhood for this vertex
        auto one_ring = collect_one_ring(mesh, vi);

        // Function to process a single ray direction
        auto process_direction = [&](const Eigen::Vector3f& dir, const Eigen::Vector3f& origin) {
            // Cast ray to find opposite surface
            auto hit = ray_caster->cast(origin + dir * absolute_ray_offset, dir);

            if (hit.has_value() && hit->ray_depth > 0.0f) {
                float hit_depth = hit->ray_depth + absolute_ray_offset;

                // Binary search for medial axis point
                float medial_distance = find_medial_axis_distance(
                    ray_caster,
                    one_ring,
                    facets,
                    origin,
                    dir,
                    hit_depth,
                    absolute_tolerance);

                return medial_distance;
            }

            // No hit found, return default_lfs
            return options.default_lfs;
        };

        if (options.direction_mode == RayDirectionMode::Interior) {
            // Shoot inward
            Eigen::Vector3f ray_direction = -vertex_normal;
            lfs = process_direction(ray_direction, vertex_pos);
        } else if (options.direction_mode == RayDirectionMode::Exterior) {
            // Shoot outward
            Eigen::Vector3f ray_direction = vertex_normal;
            lfs = process_direction(ray_direction, vertex_pos);
        } else if (options.direction_mode == RayDirectionMode::Both) {
            // Shoot in both directions and take minimum
            Eigen::Vector3f ray_direction_out = vertex_normal;
            Eigen::Vector3f ray_direction_in = -vertex_normal;
            float lfs_positive = process_direction(ray_direction_out, vertex_pos);
            float lfs_negative = process_direction(ray_direction_in, vertex_pos);
            lfs = std::min(lfs_positive, lfs_negative);
        }

        // Store result (already clamped by initialization and process_direction fallback)
        lfs_values[vi] = static_cast<Scalar>(lfs);
    });

    // Clean up temporary vertex normal attribute
    if (owns_normal_attribute) {
        mesh.delete_attribute(normal_id);
    }

    return lfs_id;
}

#define LA_X_compute_local_feature_size(_, Scalar, Index)              \
    template LA_RAYCASTING_API AttributeId compute_local_feature_size( \
        SurfaceMesh<Scalar, Index>&,                                   \
        const LocalFeatureSizeOptions&,                                \
        const RayCaster*);
LA_SURFACE_MESH_X(compute_local_feature_size, 0)

} // namespace lagrange::raycasting
