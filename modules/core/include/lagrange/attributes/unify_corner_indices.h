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

#include <tbb/parallel_for.h>
#include <tbb/parallel_sort.h>

#include <lagrange/Logger.h>
#include <lagrange/Mesh.h>
#include <lagrange/common.h>
#include <lagrange/create_mesh.h>

namespace lagrange {

/**
 * Generate a mapping from mesh corner to a unified index buffer that can be used for positions and
 * all attributes listed in the variable indexed_attribute_names.
 */
template <typename MeshType>
auto unify_corner_indices(
    MeshType& mesh,
    const std::vector<std::string>& indexed_attribute_names,
    std::vector<typename MeshType::Index>& corner_to_unified_index) -> typename MeshType::Index
{
    using AttributeArray = typename MeshType::AttributeArray;
    using Index = typename MeshType::Index;
    using IndexArray = typename MeshType::IndexArray;

    const Index num_attrs = safe_cast<Index>(indexed_attribute_names.size());
    std::vector<std::tuple<const AttributeArray&, const IndexArray&>> attrs;
    attrs.reserve(num_attrs);
    for (const auto& attr_name : indexed_attribute_names) {
        attrs.emplace_back(mesh.get_indexed_attribute(attr_name));
    }

    const Index num_vertices = mesh.get_num_vertices();
    const Index num_facets = mesh.get_num_facets();
    const Index vertex_per_facet = mesh.get_vertex_per_facet();
    const auto& facets = mesh.get_facets();
    const Index num_corners = num_facets * vertex_per_facet;
    std::vector<Index> corner_indices(num_corners);
    std::iota(corner_indices.begin(), corner_indices.end(), 0);

    // Lexicographical comparison of corner indices.
    auto corner_index_comp = [&](Index i, Index j) -> bool {
        const Index fi = i / vertex_per_facet;
        const Index ci = i % vertex_per_facet;
        const Index fj = j / vertex_per_facet;
        const Index cj = j % vertex_per_facet;

        if (facets(fi, ci) == facets(fj, cj)) {
            for (auto k : range(num_attrs)) {
                const auto& indices = std::get<1>(attrs[k]);
                if (indices(fi, ci) != indices(fj, cj)) {
                    return indices(fi, ci) < indices(fj, cj);
                }
            }
        } else {
            return facets(fi, ci) < facets(fj, cj);
        }
        return false;
    };

    auto corner_index_eq = [&](Index i, Index j) -> bool {
        const Index fi = i / vertex_per_facet;
        const Index ci = i % vertex_per_facet;
        const Index fj = j / vertex_per_facet;
        const Index cj = j % vertex_per_facet;

        if (facets(fi, ci) == facets(fj, cj)) {
            for (auto k : range(num_attrs)) {
                const auto& indices = std::get<1>(attrs[k]);
                if (indices(fi, ci) != indices(fj, cj)) {
                    return false;
                }
            }
        } else {
            return false;
        }
        return true;
    };

    tbb::parallel_sort(corner_indices.begin(), corner_indices.end(), corner_index_comp);

    std::vector<bool> visited(num_vertices, false);
    Index num_unified_vertices = num_vertices;
    corner_to_unified_index.resize(num_corners);

    for (Index i = 0; i < num_corners;) {
        const Index fi = corner_indices[i] / vertex_per_facet;
        const Index ci = corner_indices[i] % vertex_per_facet;

        Index vi = facets(fi, ci);
        if (!visited[vi]) {
            visited[vi] = true;
        } else {
            vi = num_unified_vertices++;
        }

        Index j = i;
        while (j < num_corners && corner_index_eq(corner_indices[i], corner_indices[j])) {
            corner_to_unified_index[corner_indices[j]] = vi;
            ++j;
        }
        i = j;
    }

    return num_unified_vertices;
}

} // namespace lagrange
