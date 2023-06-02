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

#include <lagrange/MeshTrait.h>
#include <lagrange/attributes/map_attributes.h>
#include <lagrange/common.h>
#include <lagrange/create_mesh.h>

#include <array>
#include <cassert>
#include <list>
#include <unordered_map>

namespace lagrange {

/**
 * Removal all duplicate facets from the mesh.
 *
 * Arguments:
 *     input_mesh
 *
 * Returns:
 *     output_mesh without any duplicate facets.
 *
 * All vertex/facet/corner attributes are mapped from the input to the
 * output.  For facet/corner attributes, only the attributes for one of the
 * duplicate facets are used in the output.  The ordering of the facets may
 * change even if the input contains no duplicate facets.
 */
template <typename MeshType>
std::unique_ptr<MeshType> remove_duplicate_facets(const MeshType& mesh)
{
    static_assert(MeshTrait<MeshType>::is_mesh(), "Input type is not Mesh");
    using Index = typename MeshType::FacetArray::Scalar;
    using Facet = std::array<Index, 3>;

    const Index num_facets = mesh.get_num_facets();
    const auto& facets = mesh.get_facets();

    auto facet_hash = [](const Facet& f) { return f[0] + f[1] + f[2]; };
    auto facet_eq = [](const Facet& f1, const Facet& f2) {
        return f1[0] == f2[0] && f1[1] == f2[1] && f1[2] == f2[2];
    };

    std::unordered_map<Facet, std::list<Index>, decltype(facet_hash), decltype(facet_eq)> facet_map(
        num_facets,
        facet_hash,
        facet_eq);
    for (Index i = 0; i < num_facets; i++) {
        Facet f{facets(i, 0), facets(i, 1), facets(i, 2)};
        std::sort(f.begin(), f.end());
        auto itr = facet_map.find(f);
        if (itr == facet_map.end()) {
            facet_map.insert({f, {i}});
        } else {
            itr->second.push_back(i);
        }
    }

    const auto num_unique_facets = facet_map.size();
    typename MeshType::FacetArray unique_facets(num_unique_facets, 3);
    std::vector<Index> ori_facet_indices(num_unique_facets);
    Index count = 0;
    for (const auto& entry : facet_map) {
        const auto& fs = entry.second;
        assert(fs.size() > 0);
        auto fid = fs.front();
        if (fs.size() > 1) {
            // Orientation votes.  Use the orientation with most votes.
            int orientation = 0;
            Index inverted_fid = fid;
            for (const auto fj : fs) {
                if ((facets(fj, 0) == facets(fid, 0) && facets(fj, 1) == facets(fid, 1) &&
                     facets(fj, 2) == facets(fid, 2)) ||
                    (facets(fj, 0) == facets(fid, 1) && facets(fj, 1) == facets(fid, 2) &&
                     facets(fj, 2) == facets(fid, 0)) ||
                    (facets(fj, 0) == facets(fid, 2) && facets(fj, 1) == facets(fid, 0) &&
                     facets(fj, 2) == facets(fid, 1))) {
                    orientation += 1;
                } else {
                    orientation -= 1;
                    inverted_fid = fid;
                }
            }
            if (orientation < 0) {
                fid = inverted_fid;
            }
        }
        unique_facets.row(count) = facets.row(fid);
        ori_facet_indices[count] = fid;
        count++;
    }

    const auto vertices = mesh.get_vertices();
    auto out_mesh = create_mesh(std::move(vertices), std::move(unique_facets));

    map_attributes(mesh, *out_mesh, {}, ori_facet_indices);

    return out_mesh;
}
} // namespace lagrange
