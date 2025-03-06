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

struct UnflipUVOptions
{
    /// Name of the attribute containing the UV coordinates.
    /// If empty, the first indexed UV attribute will be used.
    std::string_view uv_attribute_name = "";
};

///
/// Unflip flipped UV triangles.
///
/// For each flipped UV triangle, the algorithm computes an optimal (negative) scaling factor in U
/// or V coordinates, and applies it to the UV coordinates of the triangle. The triangle may be
/// detached from neighboring triangles if necessary.
///
/// @param mesh    Input mesh, which will be modified in place
/// @param options Unflip option settings
///
/// @note This algorithm is specifically designed to correct UV triangles that are flipped due to
/// periodic parameterization, such as the cylindrical parameterization of a cylinder, which can
/// result in a strip of flipped UV triangles. It is not intended for handling generic flipped UV
/// triangles. This function should be called after `rescale_uv_charts`.
///
/// @see rescale_uv_charts
///
template <typename Scalar, typename Index>
void unflip_uv_triangles(SurfaceMesh<Scalar, Index>& mesh, const UnflipUVOptions& options = {});

} // namespace lagrange
