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
#include <lagrange/raycasting/Options.h>
#include <lagrange/raycasting/api.h>

namespace lagrange::raycasting {

class RayCaster;

///
/// @addtogroup group-raycasting
/// @{
///

///
/// Project vertex attributes from one mesh to another, by projecting target vertices along a
/// prescribed direction, and interpolating surface values from facet corners of the source mesh.
///
/// By default, vertex positions are projected. Additional attributes to project can be specified via
/// `options.attribute_ids`. Set `options.project_vertices` to false to skip vertex positions.
///
/// @param[in]     source          Source mesh (must be a triangle mesh).
/// @param[in,out] target          Target mesh to be modified.
/// @param[in]     options         Projection options.
/// @param[in]     ray_caster      If provided, use this ray caster to perform the queries. The
///                                source mesh must have been added to the ray caster in advance, and
///                                the scene must have been committed. If nullptr, a temporary ray
///                                caster will be created internally.
///
/// @tparam        Scalar          Mesh scalar type.
/// @tparam        Index           Mesh index type.
///
template <typename Scalar, typename Index>
LA_RAYCASTING_API void project_directional(
    const SurfaceMesh<Scalar, Index>& source,
    SurfaceMesh<Scalar, Index>& target,
    const ProjectDirectionalOptions& options = {},
    const RayCaster* ray_caster = nullptr);

/// @}

} // namespace lagrange::raycasting
