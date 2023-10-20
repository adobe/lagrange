/*
 * Copyright 2016 Adobe. All rights reserved.
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
#include <lagrange/attributes/map_attributes.h>
#include <lagrange/common.h>
#include <lagrange/create_mesh.h>
#include <lagrange/legacy/inline.h>

namespace lagrange {
LAGRANGE_LEGACY_INLINE
namespace legacy {
/**
 * Remove topologically degenerate triangles.
 *
 * Arguments:
 *     input_mesh
 *
 * Returns:
 *     output_mesh without any topologically degnerate facets.
 *
 * All vertex and facet attributes are mapped over.
 * The output_mesh's vertices are exactly the same as input_mesh's vertices.
 * The output_mesh's facets are a subset of input_mesh's facets.
 */
template <typename MeshType>
std::unique_ptr<MeshType> remove_topologically_degenerate_triangles(const MeshType& mesh)
{
    static_assert(MeshTrait<MeshType>::is_mesh(), "Input type is not Mesh");
    la_runtime_assert(mesh.get_vertex_per_facet() == 3);

    lagrange::logger().trace("[remove_topologically_degenerate_triangles]");

    using Index = typename MeshType::FacetArray::Scalar;
    const auto& vertices = mesh.get_vertices();
    const auto& facets = mesh.get_facets();

    auto is_topologically_degenerate = [&facets](Index fid) {
        return facets(fid, 0) == facets(fid, 1) || facets(fid, 1) == facets(fid, 2) ||
               facets(fid, 2) == facets(fid, 0);
    };

    const Index num_facets = mesh.get_num_facets();
    std::vector<Index> good_triangle_indices; // this is a backward facet map
    good_triangle_indices.reserve(num_facets);
    for (Index i = 0; i < num_facets; i++) {
        if (!is_topologically_degenerate(i)) {
            good_triangle_indices.push_back(i);
        }
    }

    Index num_non_degenerate_facets = safe_cast<Index>(good_triangle_indices.size());
    typename MeshType::FacetArray good_facets(num_non_degenerate_facets, 3);
    for (Index i = 0; i < num_non_degenerate_facets; i++) {
        good_facets.row(i) = facets.row(good_triangle_indices[i]);
    }

    auto mesh2 = create_mesh(vertices, std::move(good_facets));

    // Port attributes
    map_attributes(mesh, *mesh2, {}, good_triangle_indices);

    return mesh2;
}
} // namespace legacy
} // namespace lagrange
