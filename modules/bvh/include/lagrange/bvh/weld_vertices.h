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
#pragma once

#include <lagrange/SurfaceMesh.h>
#include <lagrange/types/MappingPolicy.h>

namespace lagrange::bvh {

struct WeldOptions
{
    /// Maximum Euclidean distance between two vertices to be considered "nearby".
    float radius = 1e-6f;

    /// If true, only boundary vertices will be considered for welding.
    bool boundary_only = false;

    /// Mapping policy for float or double valued attributes
    MappingPolicy collision_policy_float = MappingPolicy::Average;

    /// Mapping policy for integral valued attributes
    MappingPolicy collision_policy_integral = MappingPolicy::KeepFirst;
};

///
/// Weld nearby vertices together of a surface mesh.
///
/// @param[in,out] mesh    The target surface mesh to be welded in place.
/// @param[in]     options Options for welding.
///
/// @warning
/// This method may lead to non-manifoldness and degeneracy in the output mesh.
///
template <typename Scalar, typename Index>
void weld_vertices(SurfaceMesh<Scalar, Index>& mesh, WeldOptions options = {});

} // namespace lagrange::bvh
