/*
 * Copyright 2026 Adobe. All rights reserved.
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
#include <lagrange/xatlas/Options.h>

#include <xatlas/xatlas.h>

#include <string_view>


namespace lagrange::xatlas::internal {

///
/// Apply xatlas output for the i-th input mesh onto a Lagrange mesh as an indexed UV attribute.
/// Preserves vertex/facet topology of `mesh`. UVs are normalized according to `policy`.
///
/// `mesh` must have the same triangle topology as the input mesh that was added to the atlas at
/// position `mesh_index`.
///
template <typename Scalar, typename Index>
void apply_atlas_uvs_to_mesh(
    SurfaceMesh<Scalar, Index>& mesh,
    const ::xatlas::Atlas& atlas,
    uint32_t mesh_index,
    MultiAtlasPolicy policy,
    std::string_view output_uv_attribute_name,
    std::string_view output_atlas_attribute_name,
    std::string_view output_chart_attribute_name);

} // namespace lagrange::xatlas::internal
