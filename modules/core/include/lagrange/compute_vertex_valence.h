/*
 * Copyright 2017 Adobe. All rights reserved.
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

#include <lagrange/Mesh.h>
#include <lagrange/MeshTrait.h>

namespace lagrange {
template <typename MeshType>
void compute_vertex_valence(MeshType& mesh)
{
    static_assert(MeshTrait<MeshType>::is_mesh(), "Input type is not Mesh");

    using Index = typename MeshType::Index;

    const Index num_vertices = mesh.get_num_vertices();
    const Index num_facets = mesh.get_num_facets();
    const Index vertex_per_facet = mesh.get_vertex_per_facet();
    const auto& facets = mesh.get_facets();

    typename MeshType::AttributeArray valence(num_vertices, 1);
    valence.setZero();

    for (Index i = 0; i < num_facets; i++) {
        for (Index j = 0; j < vertex_per_facet; j++) {
            valence(facets(i, j), 0) += 1.0;
        }
    }

    mesh.add_vertex_attribute("valence");
    mesh.import_vertex_attribute("valence", valence);
}
} // namespace lagrange
