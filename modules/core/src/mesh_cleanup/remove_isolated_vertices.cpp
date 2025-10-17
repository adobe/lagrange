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
#include <lagrange/mesh_cleanup/remove_isolated_vertices.h>

#include <lagrange/Attribute.h>
#include <lagrange/SurfaceMeshTypes.h>

namespace lagrange {

template <typename Scalar, typename Index>
void remove_isolated_vertices(SurfaceMesh<Scalar, Index>& mesh)
{
    // Remove isolated vertices
    std::vector<bool> should_remove(mesh.get_num_vertices(), true);
    for (Index v : mesh.get_corner_to_vertex().get_all()) {
        if (v != invalid<Index>()) {
            should_remove[v] = false;
        }
    }
    // Facets pointing to invalid vertices will be automatically removed as well!
    mesh.remove_vertices([&](Index v) { return should_remove[v]; });
}

#define LA_X_remove_isolated_vertices(_, Scalar, Index) \
    template LA_CORE_API void remove_isolated_vertices<Scalar, Index>(SurfaceMesh<Scalar, Index>&);
LA_SURFACE_MESH_X(remove_isolated_vertices, 0)

} // namespace lagrange
