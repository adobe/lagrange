/*
 * Copyright 2022 Adobe. All rights reserved.
 * This file is licensed to you under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License. You may obtain a copy
 * of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under
 * the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
 * OF ANY KIND, either express or implied. See the License for the specific language
 * governing permissions and limitations under the License.
 */
#include <lagrange/compute_vertex_valence.h>

#include <lagrange/Attribute.h>
#include <lagrange/SurfaceMeshTypes.h>
#include <lagrange/compute_vertex_vertex_adjacency.h>
#include <lagrange/internal/find_attribute_utils.h>
#include <lagrange/utils/assert.h>

namespace lagrange {

template <typename Scalar, typename Index>
AttributeId compute_vertex_valence(SurfaceMesh<Scalar, Index>& mesh, VertexValenceOptions options)
{
    AttributeId id = internal::find_or_create_attribute<Index, Scalar, Index>(
        mesh,
        options.output_attribute_name,
        Vertex,
        AttributeUsage::Scalar,
        1,
        internal::ResetToDefault::No);
    auto& attr = mesh.template ref_attribute<Index>(id);
    auto valence = attr.ref_all();
    la_debug_assert(static_cast<Index>(valence.size()) == mesh.get_num_vertices());

    const auto adjacency_list = compute_vertex_vertex_adjacency(mesh);
    la_debug_assert(
        static_cast<Index>(adjacency_list.get_num_entries()) == mesh.get_num_vertices());
    for (auto i : range(mesh.get_num_vertices())) {
        valence[i] = adjacency_list.get_num_neighbors(i);
    }

    return id;
}

#define LA_X_compute_vertex_valence(_, Scalar, Index) \
    template AttributeId compute_vertex_valence(SurfaceMesh<Scalar, Index>&, VertexValenceOptions);
LA_SURFACE_MESH_X(compute_vertex_valence, 0)

} // namespace lagrange
