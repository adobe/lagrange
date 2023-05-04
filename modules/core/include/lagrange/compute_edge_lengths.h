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
    #include <lagrange/legacy/compute_edge_lengths.h>
#endif

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

struct EdgeLengthOptions
{
    /// Output attribute name. If the attribute already exists, it will be overwritten.
    std::string_view output_attribute_name = "@edge_length";
};

///
/// Computes edge lengths attribute.
///
/// @tparam Scalar  Mesh scalar type
/// @tparam Index   Mesh index type
///
/// @param mesh     The input mesh
/// @param options  Options for computing edge lengths.
///
/// @return         Attribute ID of the computed edge lengths attribute.
///
template <typename Scalar, typename Index>
AttributeId compute_edge_lengths(
    SurfaceMesh<Scalar, Index>& mesh,
    const EdgeLengthOptions& options = {});

/// @}
} // namespace lagrange
