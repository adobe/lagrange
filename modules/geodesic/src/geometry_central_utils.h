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

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <geometrycentral/surface/vertex_position_geometry.h>
#include <geometrycentral/surface/surface_point.h>
#include <lagrange/utils/warnon.h>
// clang-format on

#include <memory>

namespace lagrange::geodesic::gc {

using gcSurfaceMesh = geometrycentral::surface::SurfaceMesh;
using gcGeometry = geometrycentral::surface::VertexPositionGeometry;
using gcSurfacePoint = geometrycentral::surface::SurfacePoint;

template <typename Scalar, typename Index>
std::tuple<std::unique_ptr<gcSurfaceMesh>, std::unique_ptr<gcGeometry>> extract_gc_mesh(
    SurfaceMesh<Scalar, Index>& mesh)
{
    auto vertices = vertex_view(mesh);
    auto facets = facet_view(mesh);
    auto gc_mesh = std::make_unique<gcSurfaceMesh>(facets);
    auto gc_geom = std::make_unique<gcGeometry>(*gc_mesh, vertices);
    return std::make_tuple(std::move(gc_mesh), std::move(gc_geom));
}

} // namespace lagrange::geodesic::gc
