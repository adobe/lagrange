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

#include <string_view>

namespace lagrange::packing {

/**
 * RepackOptions allows one to customize packing options.
 */
struct RepackOptions
{
    /// Name of the indexed attribute to use as UV coordinates.
    /// If empty, the first indexed UV attribute will be used.
    std::string_view uv_attribute_name = "";

    /// Name of the facet attribute that group facets into UV charts.
    /// If empty, it will be computed based on UV chart connectivity.
    std::string_view chart_attribute_name = "";

#ifndef RECTANGLE_BIN_PACK_OSS
    /// Whether to allow box to rotate by 90 degree when packing.
    bool allow_rotation = true;
#endif

    /// Should the output be normalized to fit into a unit box.
    bool normalize = true;

    /// Minimum allowed distance between two boxes normalized within [0, 1] domain.
    float margin = 1e-3f;
};

///
/// Pack UV charts of a given mesh.
///
/// @param[in,out] mesh     The mesh with UV attribute. The UV attribute will be updated in place.
/// @param[in]     options  The packing options.
///
template <typename Scalar, typename Index>
void repack_uv_charts(SurfaceMesh<Scalar, Index>& mesh, const RepackOptions& options = {});

} // namespace lagrange::packing
