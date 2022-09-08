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
#include <lagrange/common.h>
#include <lagrange/utils/safe_cast.h>

namespace lagrange {
template <typename MeshType>
void map_corner_attributes(const MeshType& from, MeshType& to)
{
    static_assert(MeshTrait<MeshType>::is_mesh(), "Input type is not Mesh");
    la_runtime_assert(from.get_num_facets() == to.get_num_facets());

    auto corner_attributes = from.get_corner_attribute_names();
    for (const auto& name : corner_attributes) {
        auto attr = from.get_corner_attribute_array(name);
        to.add_corner_attribute(name);
        to.set_corner_attribute_array(name, to_shared_ptr(attr->clone()));
    }
}

/*
map_corner_attributes assumes a backward mapping, meaning that in the
facet_map, for each element of "to", we have its respective index in "from".

You can use invert_mapping in map_attributes.h to convert a forward mapping
*/
template <typename MeshType>
void map_corner_attributes(
    const MeshType& from,
    MeshType& to,
    const std::vector<typename MeshType::Index>& facet_map)
{
    static_assert(MeshTrait<MeshType>::is_mesh(), "Input type is not Mesh");
    using Index = typename MeshType::Index;

    const Index dim = from.get_dim();
    la_runtime_assert(to.get_dim() == dim);
    la_runtime_assert(from.get_vertex_per_facet() == 3);
    la_runtime_assert(to.get_vertex_per_facet() == 3);
    la_runtime_assert(safe_cast<Index>(facet_map.size()) == to.get_num_facets());

    auto corner_map_fn = [&](Eigen::Index i,
                             std::vector<std::pair<Eigen::Index, double>>& weights) {
        const auto to_fid = i / 3;
        const auto to_cid = i % 3;
        weights.clear();
        weights.emplace_back(facet_map[to_fid] * 3 + to_cid, 1.0);
    };

    const auto to_num_corners = to.get_num_facets() * 3;
    const auto& corner_attributes = from.get_corner_attribute_names();
    for (const auto& name : corner_attributes) {
        auto attr = from.get_corner_attribute_array(name);
        auto attr2 = to_shared_ptr(attr->row_slice(to_num_corners, corner_map_fn));
        if (!to.has_corner_attribute(name)) {
            to.add_corner_attribute(name);
        }
        to.set_corner_attribute_array(name, std::move(attr2));
    }
}
} // namespace lagrange
