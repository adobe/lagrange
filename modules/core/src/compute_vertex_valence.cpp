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
#include <lagrange/internal/visit_attribute.h>
#include <lagrange/utils/Error.h>
#include <lagrange/utils/assert.h>

namespace lagrange {

template <typename Scalar, typename Index>
AttributeId compute_vertex_valence(SurfaceMesh<Scalar, Index>& mesh, VertexValenceOptions options)
{
    AttributeId id = internal::find_or_create_attribute<Index>(
        mesh,
        options.output_attribute_name,
        Vertex,
        AttributeUsage::Scalar,
        1,
        options.induced_by_attribute.empty() ? internal::ResetToDefault::No
                                             : internal::ResetToDefault::Yes);
    auto valence = mesh.template ref_attribute<Index>(id).ref_all();
    la_debug_assert(static_cast<Index>(valence.size()) == mesh.get_num_vertices());

    if (!options.induced_by_attribute.empty()) {
        // Using the graph induced by the provided edge attribute
        internal::visit_attribute_read(
            mesh,
            mesh.get_attribute_id(options.induced_by_attribute),
            [&](auto&& attr) {
                using AttributeType = std::decay_t<decltype(attr)>;
                if constexpr (!AttributeType::IsIndexed) {
                    if (attr.get_element_type() != AttributeElement::Edge) {
                        throw Error("`induced_by_attribute` must be an edge attribute");
                    }
                    auto is_candidate = attr.get_all();
                    for (Index e = 0; e < mesh.get_num_edges(); ++e) {
                        if (is_candidate[e]) {
                            auto [v0, v1] = mesh.get_edge_vertices(e);
                            ++valence[v0];
                            ++valence[v1];
                        }
                    }
                }
            });
    } else {
        // Default implementation using the vertex-vertex graph for valence computation
        const auto adjacency_list = compute_vertex_vertex_adjacency(mesh);
        la_debug_assert(
            static_cast<Index>(adjacency_list.get_num_entries()) == mesh.get_num_vertices());
        for (auto i : range(mesh.get_num_vertices())) {
            valence[i] =
                static_cast<Index>(adjacency_list.get_num_neighbors(static_cast<size_t>(i)));
        }
    }

    return id;
}

#define LA_X_compute_vertex_valence(_, Scalar, Index)        \
    template LA_CORE_API AttributeId compute_vertex_valence( \
        SurfaceMesh<Scalar, Index>&,                         \
        VertexValenceOptions);
LA_SURFACE_MESH_X(compute_vertex_valence, 0)

} // namespace lagrange
