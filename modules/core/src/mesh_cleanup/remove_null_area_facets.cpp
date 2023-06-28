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

#include <lagrange/SurfaceMeshTypes.h>
#include <lagrange/compute_area.h>
#include <lagrange/mesh_cleanup/remove_isolated_vertices.h>
#include <lagrange/mesh_cleanup/remove_null_area_facets.h>
#include <lagrange/views.h>

namespace lagrange {

template <typename Scalar, typename Index>
void remove_null_area_facets(
    SurfaceMesh<Scalar, Index>& mesh,
    const RemoveNullAreaFacetsOptions& options)
{
    auto id = compute_facet_area(mesh);
    auto area = attribute_vector_view<Scalar>(mesh, id);
    mesh.remove_facets([&](Index fi) { return std::abs(area[fi]) <= options.null_area_threshold; });

    if (options.remove_isolated_vertices) {
        remove_isolated_vertices(mesh);
    }
}

#define LA_X_remove_null_area_facets(_, Scalar, Index)    \
    template void remove_null_area_facets<Scalar, Index>( \
        SurfaceMesh<Scalar, Index>&,                      \
        const RemoveNullAreaFacetsOptions&);
LA_SURFACE_MESH_X(remove_null_area_facets, 0)

} // namespace lagrange
