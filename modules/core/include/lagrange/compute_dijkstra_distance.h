/*
 * Copyright 2017 Adobe. All rights reserved.
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
    #include <lagrange/legacy/compute_dijkstra_distance.h>
#endif

#include <lagrange/SurfaceMesh.h>
#include <lagrange/utils/SmallVector.h>
#include <optional>

namespace lagrange {

///
/// Option struct for compute_dijkstra_distance
///
template <typename Scalar, typename Index>
struct DijkstraDistanceOptions
{
    /// Seed facet index
    Index seed_facet = invalid<Index>();

    /// Seed facet barycentric coordinate
    SmallVector<Scalar, 3> barycentric_coords;

    /// Maximum radius of the dijkstra distance
    Scalar radius = 0.0;

    /// Output attribute name for dijkstra distance.
    std::string_view output_attribute_name = "@dijkstra_distance";

    /// Output involved vertices
    bool output_involved_vertices = false;
};


///
/// Computes dijkstra distance from a seed facet.
///
/// @param mesh Input mesh.
/// @param options Options for computing dijkstra distance.
///
/// @tparam     Scalar  Mesh scalar type.
/// @tparam     Index   Mesh index type.
///
/// @return Optionally, a vector of indices of vertices involved
///
template <typename Scalar, typename Index>
std::optional<std::vector<Index>> compute_dijkstra_distance(
    SurfaceMesh<Scalar, Index>& mesh,
    const DijkstraDistanceOptions<Scalar, Index>& options = {});


} // namespace lagrange
