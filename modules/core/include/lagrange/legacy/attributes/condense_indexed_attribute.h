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

#include <algorithm>
#include <numeric>
#include <string>
#include <vector>

#include <tbb/blocked_range.h>
#include <tbb/parallel_for.h>
#include <tbb/parallel_sort.h>

#include <lagrange/Mesh.h>
#include <lagrange/MeshTrait.h>
#include <lagrange/legacy/inline.h>
#include <lagrange/utils/assert.h>
#include <lagrange/utils/range.h>

namespace lagrange {
LAGRANGE_LEGACY_INLINE
namespace legacy {

/**
 * Condense indexed attribute reduces the attribute value array size by
 * eliminating locally duplicate attribute values.
 */
template <typename MeshType>
void condense_indexed_attribute(
    MeshType& mesh,
    const std::string& attr_name,
    const std::string& new_attr_name = "")
{
    static_assert(MeshTrait<MeshType>::is_mesh(), "MeshType is not a mesh");
    la_runtime_assert(
        mesh.has_indexed_attribute(attr_name),
        fmt::format("Missing attribute '{}'", attr_name));

    using Index = typename MeshType::Index;
    using AttributeArray = typename MeshType::AttributeArray;
    using IndexArray = typename MeshType::IndexArray;
    static_assert(AttributeArray::IsRowMajor, "Attribute array must be row major");

    const auto num_vertices = mesh.get_num_vertices();
    const auto num_facets = mesh.get_num_facets();
    const auto& facets = mesh.get_facets();
    const auto vertex_per_facet = mesh.get_vertex_per_facet();

    const auto attr = mesh.get_indexed_attribute(attr_name);
    const auto& attr_values = std::get<0>(attr);
    const auto& attr_indices = std::get<1>(attr);
    const Index num_cols = safe_cast<Index>(attr_values.cols());

    using IndexEntry = std::array<Index, 2>; // facet id, corner id

    /**
     * Comparison operator for index pairs.
     * Return true iff the first index pair is less than the second one
     * lexicographically.  This method assumes AttributeArray is row major.
     */
    auto index_pair_comp = [&](const IndexEntry& id0, const IndexEntry& id1) {
        const Index offset_0 = attr_indices(id0[0], id0[1]) * num_cols;
        const Index offset_1 = attr_indices(id1[0], id1[1]) * num_cols;
        const auto base_ptr = attr_values.data();
        return std::lexicographical_compare(
            base_ptr + offset_0,
            base_ptr + offset_0 + num_cols,
            base_ptr + offset_1,
            base_ptr + offset_1 + num_cols);
    };

    /**
     * Equality comparison operator between index pairs.
     */
    auto index_pair_eq = [&](const IndexEntry& id0, const IndexEntry& id1) -> bool {
        return attr_values.row(attr_indices(id0[0], id0[1])) ==
               attr_values.row(attr_indices(id1[0], id1[1]));
    };

    Index count = 0;
    std::vector<Index> condensed_attr_values_index;
    condensed_attr_values_index.reserve(attr_values.rows());
    AttributeArray condensed_attr_values;
    IndexArray condensed_attr_indices(num_facets, vertex_per_facet);
    std::vector<IndexEntry> indices(num_facets * vertex_per_facet);
    std::vector<Index> offsets(num_vertices + 2, 0);

    for (auto fi : range(num_facets)) {
        for (auto ci : range(vertex_per_facet)) {
            offsets[facets(fi, ci) + 2]++;
        }
    }
    std::partial_sum(offsets.begin(), offsets.end(), offsets.begin());

    for (auto fi : range(num_facets)) {
        for (auto ci : range(vertex_per_facet)) {
            auto vi = facets(fi, ci);
            indices[offsets[vi + 1]++] = {fi, ci};
        }
    }

    tbb::parallel_for(
        tbb::blocked_range<Index>(0, num_vertices),
        [&](const tbb::blocked_range<Index>& tbb_range) {
            for (auto vi = tbb_range.begin(); vi != tbb_range.end(); vi++) {
                tbb::parallel_sort(
                    indices.data() + offsets[vi],
                    indices.data() + offsets[vi + 1],
                    index_pair_comp);
            }
        });

    for (auto vi : range(num_vertices)) {
        const IndexEntry* local_indices = indices.data() + offsets[vi];
        const Index num_adj_facets = safe_cast<Index>(offsets[vi + 1] - offsets[vi]);
        for (Index i = 0; i < num_adj_facets; i++) {
            const auto& curr_entry = local_indices[i];
            condensed_attr_indices(curr_entry[0], curr_entry[1]) = count;

            for (Index j = i; j < num_adj_facets; j++) {
                const auto& next_entry = local_indices[j];
                if (index_pair_eq(curr_entry, next_entry)) {
                    condensed_attr_indices(next_entry[0], next_entry[1]) = count;
                    i = j;
                } else {
                    break;
                }
            }
            condensed_attr_values_index.push_back(attr_indices(curr_entry[0], curr_entry[1]));
            count++;
        }
    }

    assert(safe_cast<Index>(condensed_attr_values_index.size()) == count);

    condensed_attr_values.resize(count, num_cols);
    for (auto i : range(count)) {
        condensed_attr_values.row(i) = attr_values.row(condensed_attr_values_index[i]);
    }

    if (new_attr_name == "" || new_attr_name == attr_name) {
        mesh.import_indexed_attribute(attr_name, condensed_attr_values, condensed_attr_indices);
    } else {
        mesh.add_indexed_attribute(new_attr_name);
        mesh.import_indexed_attribute(new_attr_name, condensed_attr_values, condensed_attr_indices);
    }
}

} // namespace legacy
} // namespace lagrange
