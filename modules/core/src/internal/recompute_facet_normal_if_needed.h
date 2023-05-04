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
#include <lagrange/compute_facet_normal.h>
#include <lagrange/internal/find_attribute_utils.h>

namespace lagrange::internal {

///
/// Compute facet normal attribute if necessary or as requested.
///
/// @tparam Scalar  Mesh scalar type
/// @tparam Index   Mesh index type
///
/// @param mesh                         The input mesh.
/// @param facet_normal_attribute_name  The facet normal attribute name.
/// @param recompute_facet_normals      Whether to recompute facet normals if one already exists.
///
/// @return A pair of the facet normal attribute id and a boolean indicating whether the facet
///
template <typename Scalar, typename Index>
auto recompute_facet_normal_if_needed(
    SurfaceMesh<Scalar, Index>& mesh,
    std::string_view facet_normal_attribute_name,
    bool recompute_facet_normals)
{
    AttributeId facet_normal_id;
    bool had_facet_normals = mesh.has_attribute(facet_normal_attribute_name);
    if (recompute_facet_normals || !had_facet_normals) {
        FacetNormalOptions facet_normal_options;
        facet_normal_options.output_attribute_name = facet_normal_attribute_name;
        facet_normal_id = compute_facet_normal(mesh, facet_normal_options);
    } else {
        facet_normal_id = internal::find_attribute<Scalar>(
            mesh,
            facet_normal_attribute_name,
            Facet,
            AttributeUsage::Normal,
            3);
    }
    return std::make_pair(facet_normal_id, had_facet_normals);
}

} // namespace lagrange::internal
