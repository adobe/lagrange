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

namespace lagrange {

///
/// Split mesh facets based on material labels.
///
/// @param mesh                    Mesh on which material segmentation will be applied in place.
/// @param material_attribute_name Name of the material attribute to use for splitting.
///
/// @note The attribute should be a vertex attribute with k channels, each channel encodes the
/// probability of the vertex being part of material i (0 <= i < k). The most probable material
/// will be assigned to each vertex, and facets will be split at the boundaries of different
/// materials.
///
/// @tparam Scalar                 Mesh scalar type.
/// @param Index                   Mesh index type.
///
template <typename Scalar, typename Index>
void split_facets_by_material(
    SurfaceMesh<Scalar, Index>& mesh,
    std::string_view material_attribute_name);

} // namespace lagrange
