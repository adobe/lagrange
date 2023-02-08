/*
 * Copyright 2023 Adobe. All rights reserved.
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
#include <lagrange/utils/span.h>

namespace lagrange {
///
/// @defgroup   group-surfacemesh-utils Mesh utilities
/// @ingroup    group-surfacemesh
///
/// Various mesh processing utilities.
///
/// @{

/**
 * Remap vertices options.
 */
struct RemapVerticesOptions
{
    /// Collision policy control the behavior when two or more vertices are mapped into the same
    /// output vertex.
    enum class CollisionPolicy {
        /// Take the average of all involved vertices.
        Average,

        /// Keep the value of the first vertex.
        KeepFirst,

        /// Throw error if collision is detected.
        Error,
    };

    /// Collision policy for float or double valued attributes
    CollisionPolicy collision_policy_float = CollisionPolicy::Average;

    /// Collision policy for integral valued attributes
    CollisionPolicy collision_policy_integral = CollisionPolicy::KeepFirst;
};

/**
 * Remap vertices of a mesh based on provided forward mapping.
 *
 * @param[in,out]  mesh             The target mesh.
 * @param[in]      forward_mapping  Vertex mapping where vertex `i` will be remapped to
 *                                  vertex `forward_mapping[i]`.
 *
 * @note
 *   * All vertex attributes will be updated.
 *   * The order of facets are unchanged.
 *   * If two vertices are mapped to the same index, they will be merged.
 *   * `forward_mapping` must be surjective.
 *   * Edge information will be recomputed if present.
 *
 * @see permute_vertices for simply permuting the vertex order.
 */
template <typename Scalar, typename Index>
void remap_vertices(
    SurfaceMesh<Scalar, Index>& mesh,
    span<const Index> forward_mapping,
    RemapVerticesOptions options = {});

/// @}

} // namespace lagrange
