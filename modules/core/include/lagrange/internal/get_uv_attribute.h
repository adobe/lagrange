#pragma once
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

#include <lagrange/SurfaceMesh.h>
#include <lagrange/views.h>

#include <string_view>
#include <tuple>

namespace lagrange::internal {

///
/// Get the ID of the UV attribute of a mesh.
///
/// @param mesh The mesh to get the UV attribute from.
/// @param uv_attribute_name The name of the UV attribute. If empty, use the first indexed or vertex
/// UV attribute.
///
/// @return The ID of the UV attribute.
///
template <typename Scalar, typename Index>
AttributeId get_uv_id(
    const SurfaceMesh<Scalar, Index>& mesh,
    std::string_view uv_attribute_name = "");

///
/// Get the constant UV attribute buffers of a mesh.
///
/// @param mesh The mesh to get the UV attribute from.
/// @param uv_attribute_name The name of the UV attribute. If empty, use the first indexed or vertex UV attribute.
///
/// @return A tuple containing the UV values and indices.
///
template <typename Scalar, typename Index>
std::tuple<ConstRowMatrixView<Scalar>, ConstRowMatrixView<Index>> get_uv_attribute(
    const SurfaceMesh<Scalar, Index>& mesh,
    std::string_view uv_attribute_name = "");

///
/// Get the modifiable UV attribute buffers of a mesh.
///
/// @param mesh The mesh to get the UV attribute from.
/// @param uv_attribute_name The name of the UV attribute. If empty, use the first indexed or vertex UV attribute.
///
/// @return A tuple containing the UV values and indices.
///
template <typename Scalar, typename Index>
std::tuple<RowMatrixView<Scalar>, RowMatrixView<Index>> ref_uv_attribute(
    SurfaceMesh<Scalar, Index>& mesh,
    std::string_view uv_attribute_name = "");

} // namespace lagrange::internal
