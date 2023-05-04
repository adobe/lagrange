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
#include <lagrange/common.h>
#include <lagrange/legacy/inline.h>
#include <lagrange/utils/safe_cast.h>
#include <tbb/parallel_for.h>

namespace lagrange {
LAGRANGE_LEGACY_INLINE
namespace legacy {

template <typename MeshType>
void map_indexed_attributes(const MeshType& from, MeshType& to)
{
    static_assert(MeshTrait<MeshType>::is_mesh(), "Input type is not Mesh");
    la_runtime_assert(from.get_num_facets() == to.get_num_facets());

    using AttributeArray = typename MeshType::AttributeArray;
    using IndexArray = typename MeshType::IndexArray;

    const auto names = from.get_indexed_attribute_names();

    AttributeArray attr;
    IndexArray indices;
    for (const auto& name : names) {
        std::tie(attr, indices) = from.get_indexed_attribute(name);
        to.add_indexed_attribute(name);
        to.import_indexed_attribute(name, attr, indices);
    }
}

template <typename MeshType>
void map_indexed_attributes(
    const MeshType& from,
    MeshType& to,
    const std::vector<typename MeshType::Index>& facet_map)
{
    static_assert(MeshTrait<MeshType>::is_mesh(), "Input type is not Mesh");

    using Index = typename MeshType::Index;
    using AttributeArray = typename MeshType::AttributeArray;
    using IndexArray = typename MeshType::IndexArray;

    const auto num_out_facets = to.get_num_facets();
    const auto vertex_per_facet = to.get_vertex_per_facet();
    const auto names = from.get_indexed_attribute_names();

    AttributeArray attr;
    IndexArray from_indices, to_indices;
    for (const auto& name : names) {
        std::tie(attr, from_indices) = from.get_indexed_attribute(name);
        assert(safe_cast<Index>(from_indices.rows()) == from.get_num_facets());
        to_indices.resize(num_out_facets, vertex_per_facet);
        to_indices.setConstant(invalid<Index>());
        tbb::parallel_for(Index(0), num_out_facets, [&](Index i) {
            to_indices.row(i) = from_indices.row(facet_map[i]);
        });
        assert((to_indices.array() != invalid<Index>()).all());
        to.add_indexed_attribute(name);
        to.import_indexed_attribute(name, attr, to_indices);
    }
}

} // namespace legacy
} // namespace lagrange
