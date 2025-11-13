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
#include <lagrange/utils/StackVector.h>

namespace lagrange {

///
/// @defgroup   group-surfacemesh-utils Mesh utilities
/// @ingroup    group-surfacemesh
///
/// Various mesh processing utilities.
///
/// @{

///
/// Option struct for computing per-facet centroid.
///
struct FacetCentroidOptions
{
    /// Ouptut facet centroid attribute name.
    std::string_view output_attribute_name = "@facet_centroid";
};

///
/// Compute per-facet centroid.
///
/// @param[in,out] mesh     The input mesh.
/// @param[in]     options  Option settings to control the computation.
///
/// @tparam        Scalar   Mesh Scalar type.
/// @tparam        Index    Mesh Index type.
///
/// @return        The id of the facet centroid attribute.
/// @see           @ref FacetCentroidOptions
///
template <typename Scalar, typename Index>
AttributeId compute_facet_centroid(
    SurfaceMesh<Scalar, Index>& mesh,
    FacetCentroidOptions options = {});

///
/// Option struct for computing mesh centroid.
///
struct MeshCentroidOptions
{
    /// Weighting type for mesh centroid computation.
    enum WeightingType {
        Uniform, ///< Per-facet centroid are weighted uniformly.
        Area ///< Per-facet centroid are weighted by facet area.
    } weighting_type = Area;

    /// Precomputed facet centroid attribute name.
    /// If the attribute does not exist, a temp attribute will be computed and stored in this name.
    std::string_view facet_centroid_attribute_name = "@facet_centroid";

    /// Precomputed facet area attribute name.
    /// If the attribute does not exist, a temp attribute will be computed and stored in this name.
    std::string_view facet_area_attribute_name = "@facet_area";
};

///
/// Compute mesh centroid, where mesh centroid is defined as the weighted sum of facet centroids.
///
/// @param[in]  mesh      The input mesh.
/// @param[out] centroid  The buffer to store centroid coordinates.
/// @param[in]  options   Option settings to control the computation.
///
/// @tparam     Scalar    Mesh Scalar type.
/// @tparam     Index     Mesh Index type.
///
/// @see        @ref MeshCentroidOptions
///
template <typename Scalar, typename Index>
void compute_mesh_centroid(
    const SurfaceMesh<Scalar, Index>& mesh,
    span<Scalar> centroid,
    MeshCentroidOptions options = {});

/// @}
} // namespace lagrange
