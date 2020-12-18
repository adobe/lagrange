/*
 * Copyright 2020 Adobe. All rights reserved.
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

namespace lagrange {

template <typename MeshType>
std::unique_ptr<MeshType> remove_null_area_triangles(const MeshType& mesh)
{
    static_assert(lagrange::MeshTrait<MeshType>::is_mesh(), "Input type is not Mesh");
    assert(mesh.get_vertex_per_facet() == 3);

    using Index = typename MeshType::FacetArray::Scalar;
    const auto& vertices = mesh.get_vertices();
    const auto& facets = mesh.get_facets();

    auto has_non_null_area = [&vertices, &facets](Index fid) {
        const auto& aa = vertices.row(facets(fid, 0));
        const auto& bb = vertices.row(facets(fid, 1));
        const auto& cc = vertices.row(facets(fid, 2));
        const auto& area = (bb - aa).cross(cc - aa).norm();
        return area > 0;
    };

    const Index num_facets = mesh.get_num_facets();
    std::vector<Index> good_triangle_indices; // this is a backward facet map
    for (Index i = 0; i < num_facets; i++) {
        if (has_non_null_area(i)) {
            good_triangle_indices.push_back(i);
        }
    }

    Index num_non_degenerate_facets = lagrange::safe_cast<Index>(good_triangle_indices.size());
    typename MeshType::FacetArray good_facets(num_non_degenerate_facets, 3);
    for (Index i = 0; i < num_non_degenerate_facets; i++) {
        good_facets.row(i) = facets.row(good_triangle_indices[i]);
    }

    auto mesh_ = lagrange::create_mesh(vertices, good_facets);

    lagrange::map_attributes(mesh, *mesh_, {}, good_triangle_indices);

    return mesh_;
}

} // namespace lagrange
