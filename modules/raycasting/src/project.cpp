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

#include <lagrange/raycasting/project.h>

#include <lagrange/SurfaceMeshTypes.h>
#include <lagrange/raycasting/project_closest_point.h>
#include <lagrange/raycasting/project_closest_vertex.h>
#include <lagrange/raycasting/project_directional.h>
#include <lagrange/utils/assert.h>

#include <stdexcept>

namespace lagrange::raycasting {

template <typename Scalar, typename Index>
void project(
    const SurfaceMesh<Scalar, Index>& source,
    SurfaceMesh<Scalar, Index>& target,
    const ProjectOptions& options,
    const RayCaster* ray_caster)
{
    la_runtime_assert(source.is_triangle_mesh());

    switch (options.project_mode) {
    case ProjectMode::ClosestVertex: {
        project_closest_vertex(source, target, options, ray_caster);
        break;
    }
    case ProjectMode::ClosestPoint: {
        project_closest_point(source, target, options, ray_caster);
        break;
    }
    case ProjectMode::RayCasting: {
        project_directional(source, target, options, ray_caster);
        break;
    }
    default: throw std::runtime_error("Unknown ProjectMode");
    }
}

#define LA_X_project(_, Scalar, Index)       \
    template LA_RAYCASTING_API void project( \
        const SurfaceMesh<Scalar, Index>&,   \
        SurfaceMesh<Scalar, Index>&,         \
        const ProjectOptions&,               \
        const RayCaster*);
LA_SURFACE_MESH_X(project, 0)

} // namespace lagrange::raycasting
