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

#ifdef LAGRANGE_ENABLE_LEGACY_FUNCTIONS
    #include <lagrange/mesh_cleanup/legacy/remove_short_edges.h>
#endif

#include <lagrange/SurfaceMesh.h>
#include <lagrange/internal/constants.h>

#include <string_view>

namespace lagrange {

///
/// @addtogroup group-surfacemesh-cleanup
/// @{
///

///
/// Options for remove_short_edges function.
///
struct RemoveShortEdgesOptions
{
    /// Edge length threshold for removal. Edges with length <= threshold will be removed.
    double threshold = 0;

    /// Optional: User-defined per-vertex importance attribute name.
    /// If provided (non-empty) and exists, this attribute will be used to determine which vertex
    /// to keep during edge collapse. Higher values = more important.
    /// If empty or does not exist, importance will be computed from geometry
    /// (dihedral angles, boundary status) and stored with a temporary name.
    /// Type: Scalar (float or double)
    std::string_view vertex_importance_attribute_name = "";

    /// Maximum normal deviation (in radians) allowed for 1-ring facets of the removed vertex
    /// after an edge collapse. A collapse is skipped if any surrounding facet's normal would
    /// rotate by more than this angle. The default (pi/2) only rejects actual normal flips;
    /// tighten this value (e.g. pi/6) for stricter geometric quality.
    double max_normal_deviation_angle = lagrange::internal::pi / 2;
};

///
/// Collapse all edges shorter than a given tolerance.
///
/// @param mesh       Input mesh to be updated in place.
/// @param threshold  Edges with length <= threshold will be removed.
///
template <typename Scalar, typename Index>
void remove_short_edges(SurfaceMesh<Scalar, Index>& mesh, Scalar threshold = 0);

///
/// Collapse all edges shorter than a given tolerance.
///
/// @param mesh       Input mesh to be updated in place.
/// @param options    Options for edge removal including threshold and importance attribute.
///
template <typename Scalar, typename Index>
void remove_short_edges(SurfaceMesh<Scalar, Index>& mesh, const RemoveShortEdgesOptions& options);

/// @}

} // namespace lagrange
