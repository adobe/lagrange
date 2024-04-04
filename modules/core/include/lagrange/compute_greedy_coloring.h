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
/// Option struct for computing dihedral angles.
///
struct GreedyColoringOptions
{
    /// Output attribute name. If the attribute already exists, it will be overwritten.
    std::string_view output_attribute_name = "@color_id";

    /// Element type to be colored. Can be either Vertex or Facet.
    AttributeElement element_type = AttributeElement::Facet;

    /// Minimum number of colors to use. The algorithm will cycle through them but may use more.
    size_t num_color_used = 8;
};

///
/// Compute a greedy graph coloring of the mesh. The selected mesh element type (either Vertex or
/// Facet) will be colored in such a way that no two adjacent element share the same color.
///
/// @param[in,out] mesh     Input mesh to be colored. Modified to compute edge information and the
///                         new color attribute.
/// @param[in]     options  Coloring options.
///
/// @tparam        Scalar   Mesh scalar type.
/// @tparam        Index    Mesh index type.
///
/// @return        Id of the newly computed color attribute. The value type of the created attribute
///                will be the same as the mesh index type.
///
template <typename Scalar, typename Index>
AttributeId compute_greedy_coloring(
    SurfaceMesh<Scalar, Index>& mesh,
    const GreedyColoringOptions& options = {});

/// @}
} // namespace lagrange
