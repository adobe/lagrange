/*
 * Copyright 2022 Adobe. All rights reserved.
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
/// Option struct for computing per-corner mesh normals.
///
struct CornerNormalOptions
{
    /// Output normal attribute name.
    std::string_view output_attribute_name = "@weighted_corner_normal";

    /// Per-vertex normal averaging weighting type.
    NormalWeightingType weight_type = NormalWeightingType::Uniform;
};

/**
 * Compute corner normals.
 *
 * @param[in]  mesh    The input mesh.
 * @param[in]  option  Optional arguments to control normal generation.
 *
 * @tparam     Scalar  Mesh scalar type.
 * @tparam     Index   Mesh index type.
 *
 * @return     The corner attribute id of corner normal attribute.
 *
 * @note       If `options.weight_type` is not `Uniform`, the resulting corner normal's lengths
 *             encodes the corresponding weight.
 *
 * @note       Corner normals around a given vertex could be different even when the vertex is at a
 *             smooth region.  For computing smooth normal, use @ref compute_normal instead.
 *
 * @see        `CornerNormalOptions`, `compute_normal`.
 */
template <typename Scalar, typename Index>
AttributeId compute_weighted_corner_normal(
    SurfaceMesh<Scalar, Index>& mesh,
    CornerNormalOptions option = {});

/// @}
} // namespace lagrange
