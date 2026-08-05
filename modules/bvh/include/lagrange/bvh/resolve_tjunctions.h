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
#include <lagrange/bvh/api.h>

namespace lagrange::bvh {

/// @addtogroup module-bvh
/// @{

///
/// Option settings for @ref resolve_tjunctions.
///
struct ResolveTJunctionsOptions
{
    /// Absolute distance tolerance used to detect T-junctions. A vertex whose distance to a
    /// non-incident edge is within this tolerance, and whose projection lies strictly between the
    /// edge endpoints, is treated as lying on that edge. If negative, the tolerance defaults to
    /// `1e-6` times the bounding box diagonal of the mesh.
    double tolerance = -1;

    /// If true, only boundary edges (edges adjacent to a single facet) are checked for
    /// T-junctions. Set to false to also resolve T-junctions on interior edges.
    bool boundary_only = true;

    /// If true (default), triangulate the facets affected by edge splitting, so a triangle-mesh
    /// input yields a triangle-mesh output. If false, leave those facets as polygons.
    bool triangulate_affected = true;
};

///
/// Resolve T-junctions formed by collinear, overlapping edges.
///
/// A T-junction occurs when a vertex lies on an edge that it is not topologically connected to,
/// causing the edge to overlap with the (unconnected) sub-edges incident to that vertex. This
/// function splits every such edge at the vertices lying on it, and splits the adjacent facets
/// accordingly, so the output mesh contains no overlapping collinear edges.
///
/// Vertices are not moved: only edges and facets are subdivided so the topology conforms to the
/// existing vertex positions. The `tolerance` only controls detection, not geometric snapping.
///
/// Both triangle and polygonal meshes are supported. By default (`triangulate_affected == true`)
/// the facets touched by a split are triangulated, so a triangle-mesh input yields a triangle-mesh
/// output. Set `triangulate_affected` to false to instead keep those facets as polygons, with the
/// split points inserted as additional (collinear) boundary vertices.
///
/// @param[in,out] mesh    Input mesh (triangle or polygonal). Modified in place.
/// @param[in]     options Optional settings.
///
/// @tparam        Scalar  Mesh scalar type.
/// @tparam        Index   Mesh index type.
///
/// @note Consider running @ref remove_duplicate_vertices and @ref remove_degenerate_facets
///       beforehand to avoid propagating pre-existing degeneracies.
///
template <typename Scalar, typename Index>
void resolve_tjunctions(SurfaceMesh<Scalar, Index>& mesh, ResolveTJunctionsOptions options = {});

/// @}

} // namespace lagrange::bvh
