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

#ifdef LAGRANGE_ENABLE_LEGACY_FUNCTIONS
    #include <lagrange/mesh_cleanup/legacy/remove_duplicate_facets.h>
#endif

#include <lagrange/SurfaceMesh.h>

namespace lagrange {

///
/// Options for remove_duplicate_facets
///
struct RemoveDuplicateFacetOptions
{
    /// If true, facets with opposite orientations (e.g. (0, 1, 2) and (2, 1, 0)) are considered as
    /// non-duplicates.
    bool consider_orientation = false;
};

///
/// Remove duplicate facets in the mesh.
///
/// @tparam Scalar              Mesh scalar type
/// @tparam Index               Mesh index type
///
/// @paramp[in,out] mesh        Input mesh
/// @param[in]      opts        Options
///
/// @note If `opts.consider_orientation` is false, facets with opposite orientations (e.g. (0, 1, 2)
/// and (2, 1, 0)) are considered as duplicates. If the two orientations have equal number of
/// duplicate facets, all of them will be removed. Otherwise, only one facet with the majority
/// orientation will be kept.
///
template <typename Scalar, typename Index>
void remove_duplicate_facets(
    SurfaceMesh<Scalar, Index>& mesh,
    const RemoveDuplicateFacetOptions& opts = {});

} // namespace lagrange
