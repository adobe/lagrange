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
    #include <lagrange/legacy/compute_vertex_normal.h>
#endif

#include <lagrange/NormalWeightingType.h>
#include <lagrange/SurfaceMesh.h>

#include <string_view>

namespace lagrange {

///
/// @defgroup   group-surfacemesh-utils Mesh utilities
/// @ingroup    group-surfacemesh
///
/// Various mesh processing utilities.
///
/// @{

///
/// Option struct for computing per-vertex mesh normals.
///
struct VertexNormalOptions
{
    /// Output normal attribute name.
    std::string_view output_attribute_name = "@vertex_normal";

    /// Per-vertex normal averaging weighting type.
    NormalWeightingType weight_type = NormalWeightingType::Angle;

    /// Precomputed weighted corner attribute name. If the attribute does not exist, the algorithm
    /// will compute it. Note that if the input mesh has edge information, this intermediate
    /// attribute will not be computed (averaging will be performed directly in parallel).
    std::string_view weighted_corner_normal_attribute_name = "@weighted_corner_normal";

    /// Whether to recompute the weighted corner normal attribute, or reuse existing cached values
    /// if present.
    bool recompute_weighted_corner_normals = false;

    /// Whether to keep any newly added weighted corner normal attribute. If such an attribute is
    /// already present in the input mesh, it will not be removed, even if this argument is set to
    /// false.
    bool keep_weighted_corner_normals = false;
};

/**
 * Compute per-vertex normals based on specified weighting type.
 *
 * @param[in]  mesh     The input mesh.
 * @param[in]  options  Optional arguments to control normal generation.
 *
 * @tparam     Scalar   Mesh scalar type.
 * @tparam     Index    Mesh index type.
 *
 * @return     The attribute id of vertex normal attribute.
 *
 * @see        `VertexNormalOptions`.
 */
template <typename Scalar, typename Index>
AttributeId compute_vertex_normal(
    SurfaceMesh<Scalar, Index>& mesh,
    VertexNormalOptions options = {});

/// @}

} // namespace lagrange
