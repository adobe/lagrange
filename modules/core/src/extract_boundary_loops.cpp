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
#include <lagrange/extract_boundary_loops.h>
#include <lagrange/utils/chain_edges.h>

#include <array>
#include <vector>

namespace lagrange {

template <typename Scalar, typename Index>
std::vector<std::vector<Index>> extract_boundary_loops(const SurfaceMesh<Scalar, Index>& mesh)
{
    if (!mesh.has_edges()) {
        auto mesh_copy = mesh;
        mesh_copy.initialize_edges();
        return extract_boundary_loops(mesh_copy);
    }

    const auto num_edges = mesh.get_num_edges();
    std::vector<Index> bd_edges;
    bd_edges.reserve(num_edges * 2);

    for (Index i = 0; i < num_edges; i++) {
        if (mesh.is_boundary_edge(i)) {
            auto e = mesh.get_edge_vertices(i);
            bd_edges.push_back(e[0]);
            bd_edges.push_back(e[1]);
        }
    }

    auto result = chain_directed_edges<Index>({bd_edges.data(), bd_edges.size()});
    const auto& loops = result.loops;
    const auto& chains = result.chains;
    if (chains.size() > 1) {
        logger().warn(
            "Mesh boundary is not simple: {} loops and {} chains",
            loops.size(),
            chains.size());
    }
    return std::move(result.loops);
}

#define LA_X_extract_boundary_loops(_, Scalar, Index)                               \
    template LA_CORE_API std::vector<std::vector<Index>> extract_boundary_loops<Scalar, Index>( \
        const SurfaceMesh<Scalar, Index>& mesh);
LA_SURFACE_MESH_X(extract_boundary_loops, 0)

} // namespace lagrange
