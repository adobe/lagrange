/*
 * Copyright 2018 Adobe. All rights reserved.
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

#include <lagrange/Edge.h>
#include <lagrange/MeshTrait.h>

namespace lagrange {
/*
return Euler characteristic of the mesh
*/
template <typename MeshType>
int compute_euler(MeshType& mesh)
{
    static_assert(MeshTrait<MeshType>::is_mesh(), "Input type is not Mesh");
    mesh.initialize_edge_data();
    return (int)mesh.get_num_vertices() + (int)mesh.get_num_facets() - (int)mesh.get_num_edges();
}
} // namespace lagrange
