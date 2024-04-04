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

#include <lagrange/Logger.h>
#include <lagrange/SurfaceMeshTypes.h>
#include <lagrange/mesh_cleanup/remove_topologically_degenerate_facets.h>
#include <lagrange/utils/assert.h>

namespace lagrange {

template <typename Scalar, typename Index>
void remove_topologically_degenerate_facets(SurfaceMesh<Scalar, Index>& mesh)
{
    if (!mesh.is_triangle_mesh()) {
        logger().warn("Non-triangle facets are not checked for topological degeneracy.");
    }

    auto is_topologically_degenerate = [&](Index fid) {
        auto facet = mesh.get_facet_vertices(fid);
        const size_t n = facet.size();
        if (n != 3) return false;

        if (facet[0] == facet[1] || facet[0] == facet[2] || facet[1] == facet[2])
            return true;
        else
            return false;
    };
    mesh.remove_facets(is_topologically_degenerate);
}

#define LA_X_remove_topologically_degenerate_facets(_, Scalar, Index)    \
    template LA_CORE_API void remove_topologically_degenerate_facets<Scalar, Index>( \
        SurfaceMesh<Scalar, Index>&);
LA_SURFACE_MESH_X(remove_topologically_degenerate_facets, 0)

} // namespace lagrange
