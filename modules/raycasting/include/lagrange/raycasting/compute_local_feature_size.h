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
#pragma once

#include <lagrange/SurfaceMesh.h>
#include <lagrange/raycasting/api.h>

#include <limits>
#include <string_view>

namespace lagrange::raycasting {

class RayCaster;

///
/// @addtogroup group-raycasting
/// @{
///

///
/// Ray direction mode for local feature size computation.
///
enum class RayDirectionMode {
    /// Cast rays into the interior of the shape (hemisphere bounded by 1-ring normals, pointing
    /// inward).
    Interior,

    /// Cast rays to the exterior of the shape (hemisphere bounded by 1-ring normals, pointing
    /// outward).
    Exterior,

    /// Cast rays in all directions (full sphere).
    Both,
};

///
/// Options for compute_local_feature_size().
///
struct LocalFeatureSizeOptions
{
    /// Output attribute name for local feature size values.
    std::string_view output_attribute_name = "@lfs";

    /// Input vertex normal attribute name. If empty, vertex normals will be computed internally.
    std::string_view vertex_normal_attribute_name = "";

    /// Ray direction mode (interior, exterior, or both).
    RayDirectionMode direction_mode = RayDirectionMode::Interior;

    /// Ray offset along the vertex normal to avoid self-intersection (relative to bounding box
    /// diagonal). The actual offset distance is `ray_offset * bbox_diagonal`.
    /// Set to 0 to disable offset (not recommended).
    float ray_offset = 1e-4f;

    /// Default local feature size value used when raycasting fails to find valid hits.
    /// Default: infinity.
    float default_lfs = std::numeric_limits<float>::infinity();

    /// Error tolerance for medial axis binary search convergence (relative to bounding box diagonal).
    /// The binary search stops when |distance_to_surface - depth_along_ray| < tolerance * bbox_diagonal.
    /// Smaller values produce more accurate results but require more iterations.
    /// Default: 1e-4 (0.01% of bounding box diagonal).
    float medial_axis_tolerance = 1e-4f;
};

///
/// Compute local feature size for each vertex of a mesh using medial axis approximation.
///
/// For each vertex, this function:
/// 1. Casts a single ray along the normal direction (inward for Interior, outward for Exterior,
///    both directions for Both mode) to find the opposite surface.
/// 2. Performs binary search along the ray to find the medial axis point - approximated as the
///    largest depth from where the closest point on surface is within 1-ring of the vertex.
/// 3. Returns this distance as the local feature size.
///
/// @note       This method works best for meshes with uniformly sized triangles.
///
/// @note       For `Interior` mode, rays are cast inward (negative normal direction). For
///             `Exterior` mode, rays are cast outward (positive normal direction). For `Both`
///             mode, rays are cast in both directions and the minimum LFS is taken.
///
/// @note       If raycasting fails to find a hit, or if the binary search fails to converge,
///             the `default_lfs` value is used as a fallback.
///
/// @param[in,out] mesh          Mesh to process (must be a triangle mesh). The mesh is modified to
///                              add the local feature size attribute.
/// @param[in]     options       Options for local feature size computation.
/// @param[in]     ray_caster    If provided, use this ray caster to perform the queries. The mesh
///                              must have been added to the ray caster in advance, and the scene
///                              must have been committed. If nullptr, a temporary ray caster will
///                              be created internally.
///
/// @tparam        Scalar        Mesh scalar type.
/// @tparam        Index         Mesh index type.
///
/// @return        The attribute id of the local feature size attribute.
///
template <typename Scalar, typename Index>
LA_RAYCASTING_API AttributeId compute_local_feature_size(
    SurfaceMesh<Scalar, Index>& mesh,
    const LocalFeatureSizeOptions& options = {},
    const RayCaster* ray_caster = nullptr);

/// @}

} // namespace lagrange::raycasting
