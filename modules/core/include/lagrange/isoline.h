/*
 * Copyright 2024 Adobe. All rights reserved.
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
/// @defgroup   group-surfacemesh-utils Mesh utilities
/// @ingroup    group-surfacemesh
///
/// Various mesh processing utilities.
///
/// @{

///
/// Options for isoline extraction/trimming.
///
struct IsolineOptions
{
    /// Attribute ID of the scalar field to use. Can be a vertex or indexed attribute.
    AttributeId attribute_id = invalid_attribute_id();

    /// Channel index to use for the scalar field.
    size_t channel_index = 0;

    /// Isovalue to extract or trim with.
    double isovalue = 0.0;

    /// Whether to keep the part below the isoline. Ignored for isoline extraction.
    bool keep_below = true;
};

///
/// Trim a mesh by the isoline of an implicit function defined on the mesh vertices/corners.
///
/// @note The input must be a triangle mesh.
///
/// @param[in]  mesh     Input triangle mesh to trim.
/// @param[in]  options  Isoline options.
///
/// @tparam     Scalar   Mesh scalar type.
/// @tparam     Index    Mesh index type.
///
/// @return     The trimmed mesh.
///
template <typename Scalar, typename Index>
SurfaceMesh<Scalar, Index> trim_by_isoline(
    const SurfaceMesh<Scalar, Index>& mesh,
    const IsolineOptions& options = {});

///
/// Extract the isoline of an implicit function defined on the mesh vertices/corners.
///
/// @note The input must be a triangle mesh.
///
/// @param[in]  mesh     Input triangle mesh to extract the isoline from.
/// @param[in]  options  Isoline options.
///
/// @tparam     Scalar   Mesh scalar type.
/// @tparam     Index    Mesh index type.
///
/// @return     A mesh whose facets is a collection of size 2 elements representing the extracted isoline.
///
template <typename Scalar, typename Index>
SurfaceMesh<Scalar, Index> extract_isoline(
    const SurfaceMesh<Scalar, Index>& mesh,
    const IsolineOptions& options = {});

/// @}
} // namespace lagrange
