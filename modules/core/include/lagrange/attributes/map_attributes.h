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

#include <vector>

#include <lagrange/Mesh.h>
#include <lagrange/MeshTrait.h>
#include <lagrange/attributes/map_corner_attributes.h>
#include <lagrange/attributes/map_facet_attributes.h>
#include <lagrange/attributes/map_indexed_attributes.h>
#include <lagrange/attributes/map_vertex_attributes.h>
#include <lagrange/common.h>
#include <lagrange/utils/range.h>

namespace lagrange {

/*
Inverts a mapping from some indexes to other indexes. This mapping can be either
forward (meaning "from" to "to"), or backwards ("to" to "from").

 - map: the forward or backward map as a vector of indexes.
        The number of elements that we are mapping 'from' is the size of this vector.

 - target_count: Number of elements that we are mapping 'to'. The returned vector will have this
size.
*/
template <typename Index>
std::vector<Index> invert_mapping(const std::vector<Index>& map, Index target_count)
{
    if (map.empty()) {
        return {}; // empty vector
    }

    std::vector<Index> ret(target_count, invalid<Index>());
    for (auto i : range((Index)map.size())) {
        Index value = map[i];
        if (value != invalid<Index>()) {
            la_runtime_assert(value < target_count);
            ret[value] = i;
        }
    }
    return ret;
}

/*
Mapping defines a mapping from a lagrange::Mesh to another.

Does not hold references to the two meshes.

Holds a vector of indexes for each of vertices, facets, corners.
Those vectors can be either empty, meaning that the correspondence
has not changed and that those elements can be mapped 1 to 1.

Or, they can have the same size as the elements in a mesh, and for each
element i, the element at [i] will contain the index in the other mesh.
This index can be INVALID in case the element does not exist in the old mesh.
*/

template <typename MeshType>
struct MeshMapping
{
    using Index = typename MeshType::Index;
    std::vector<Index> vertex;
    std::vector<Index> facet;
};


// Forward mapping: ("from" mesh) --> ("to" mesh)
// For each element of "from" (vertices, facets, corners), has index of element in "to".
template <typename MeshType>
struct ForwardMeshMapping : public MeshMapping<MeshType>
{
};


// Backward mapping: ("from" mesh) <-- ("to" mesh)
// For each element of "to" (vertices, facets), has index of element in "from".
template <typename MeshType>
struct BackwardMeshMapping : public MeshMapping<MeshType>
{
};


template <typename MeshType>
MeshMapping<MeshType> invert_mapping(const MeshMapping<MeshType>& map1, const MeshType& target_mesh)
{
    static_assert(MeshTrait<MeshType>::is_mesh(), "Input type is not Mesh");
    MeshMapping<MeshType> map2;
    map2.vertex = invert_mapping(map1.vertex, target_mesh.get_num_vertices());
    map2.facet = invert_mapping(map1.facet, target_mesh.get_num_facets());
    return map2;
}

template <typename MeshType>
void map_attributes(const MeshType& from, MeshType& to, const BackwardMeshMapping<MeshType>& map)
{
    static_assert(MeshTrait<MeshType>::is_mesh(), "Input type is not Mesh");
    map_vertex_attributes(from, to, map.vertex);
    map_facet_attributes(from, to, map.facet);
    map_corner_attributes(from, to);
    map_indexed_attributes(from, to);
}

template <typename MeshType>
void map_attributes(const MeshType& from, MeshType& to, const ForwardMeshMapping<MeshType>& map)
{
    static_assert(MeshTrait<MeshType>::is_mesh(), "Input type is not Mesh");
    map_attributes(from, to, invert_mapping(map, to));
}

template <typename MeshType>
void map_attributes(
    const MeshType& from,
    MeshType& to,
    const std::vector<typename MeshType::Index>& backward_vertex_mapping = {},
    const std::vector<typename MeshType::Index>& backward_facet_mapping = {})
{
    static_assert(MeshTrait<MeshType>::is_mesh(), "Input type is not Mesh");
    map_vertex_attributes(from, to, backward_vertex_mapping);
    map_facet_attributes(from, to, backward_facet_mapping);
    if (backward_facet_mapping.empty() && to.get_num_facets() != 0) {
        map_corner_attributes(from, to);
        map_indexed_attributes(from, to);
    } else {
        map_corner_attributes(from, to, backward_facet_mapping);
        map_indexed_attributes(from, to, backward_facet_mapping);
    }
}

} // namespace lagrange
