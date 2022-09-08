/*
 * Copyright 2019 Adobe. All rights reserved.
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

#include <memory>

#include <lagrange/Edge.h>
#include <lagrange/Mesh.h>
#include <lagrange/MeshTrait.h>
#include <lagrange/attributes/eval_as_attribute.h>

namespace lagrange {
/*
Fills the edge attribute "length" with edge lengths.

Initializes the mesh edge data map if needed.
*/
template <typename MeshType>
void compute_edge_lengths(MeshType& mesh)
{
    static_assert(MeshTrait<MeshType>::is_mesh(), "Input type is not Mesh");

    using Index = typename MeshType::Index;

    mesh.initialize_edge_data();

    const auto& vertices = mesh.get_vertices();

    eval_as_edge_attribute_new(mesh, "length", [&](Index e_idx) {
        auto v = mesh.get_edge_vertices(e_idx);
        auto p0 = vertices.row(v[0]);
        auto p1 = vertices.row(v[1]);
        return (p0 - p1).norm();
    });
}
} // namespace lagrange
