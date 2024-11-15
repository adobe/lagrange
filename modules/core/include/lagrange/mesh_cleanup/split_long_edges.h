/*
 * Copyright 2016 Adobe. All rights reserved.
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
    #include <lagrange/mesh_cleanup/legacy/split_long_edges.h>
#endif

#include <lagrange/SurfaceMesh.h>

#include <string_view>


namespace lagrange {

struct SplitLongEdgesOptions
{
    /// Maximum edge length. Edges longer than this value will be split.
    float max_edge_length = 0.1f;

    /// If true, the operation will be applied recursively until no edge is longer than
    /// `max_edge_length`.
    bool recursive = true;

    /// The facet attribute name that will be used to determine the active region.
    /// Only edges that are part of the active region will be split.
    /// If empty, all edges will be considered.
    /// The attribute must be of value type `uint8_t`.
    std::string_view active_region_attribute = "";

    /// The edge length attribute name that will be used to determine the edge length.
    /// If this attribute does not exist, the function will compute the edge lengths.
    std::string_view edge_length_attribute = "@edge_length";
};

///
/// Split edges that are longer than `options.max_edge_length`.
///
/// @param[in,out] mesh    Input mesh.
/// @param[in]     options Optional settings.
///
template <typename Scalar, typename Index>
void split_long_edges(SurfaceMesh<Scalar, Index>& mesh, SplitLongEdgesOptions options = {});

} // namespace lagrange
