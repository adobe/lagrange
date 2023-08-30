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

namespace lagrange {

///
/// Option struct for remove_null_area_facets.
///
struct RemoveNullAreaFacetsOptions
{
    /// Facets with area <= null_area_threshold will be removed.
    double null_area_threshold = 0;

    /// If true, also remove isolated vertices after removing null area facets.
    bool remove_isolated_vertices = false;
};

///
/// Removes all facets with unsigned area <= options.null_area_threshold.
///
/// @tparam Scalar         Mesh scalar type.
/// @tparam Index          Mesh index type.
///
/// @param[in,out] mesh    Input mesh.
/// @param[in]     options Options settings for removing null area facets.
///
template <typename Scalar, typename Index>
void remove_null_area_facets(
    SurfaceMesh<Scalar, Index>& mesh,
    const RemoveNullAreaFacetsOptions& options = {});

} // namespace lagrange
