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

#include <Eigen/Core>

#include <lagrange/Mesh.h>
#include <lagrange/MeshTrait.h>
#include <lagrange/common.h>
#include <lagrange/utils/safe_cast.h>

namespace lagrange {

/*
map_vertex_attributes assumes a backward mapping, meaning that in the
vertex_map, for each element of "to", we have its respective index in "from".

You can use invert_mapping in map_attributes.h to convert a forward mapping
*/

template <typename MeshType1, typename MeshType2>
void map_vertex_attributes(
    const MeshType1& from,
    MeshType2& to,
    const std::vector<typename MeshType1::Index>& vertex_map = {})
{
    static_assert(MeshTrait<MeshType1>::is_mesh(), "Input type is not Mesh");
    static_assert(MeshTrait<MeshType2>::is_mesh(), "Input type is not Mesh");
    using Index = typename MeshType1::Index;
    LA_ASSERT(vertex_map.empty() || safe_cast<Index>(vertex_map.size()) == to.get_num_vertices());

    const auto& vertex_attributes = from.get_vertex_attribute_names();
    const auto num_vertices = to.get_num_vertices();
    const bool use_map = safe_cast<Index>(vertex_map.size()) == num_vertices;
    for (const auto& name : vertex_attributes) {
        auto attr = from.get_vertex_attribute_array(name);
        to.add_vertex_attribute(name);
        if (use_map) {
            auto attr2 = to_shared_ptr(attr->row_slice(vertex_map));
            to.set_vertex_attribute_array(name, std::move(attr2));
        } else {
            auto attr2 = to_shared_ptr(attr->clone());
            to.set_vertex_attribute_array(name, std::move(attr2));
        }
    }
}

} // namespace lagrange
