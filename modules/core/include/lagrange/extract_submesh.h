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

#include <memory>

#include <lagrange/Mesh.h>
#include <lagrange/MeshTrait.h>
#include <lagrange/create_mesh.h>
#include <lagrange/utils/range.h>
#include <lagrange/utils/safe_cast.h>

namespace lagrange {

template <typename MeshType>
std::vector<std::unique_ptr<MeshType>> extract_component_submeshes(
    MeshType& mesh,
    std::vector<std::vector<typename MeshType::Index>>* vertex_mappings = nullptr,
    std::vector<std::vector<typename MeshType::Index>>* facet_mappings = nullptr)
{
    static_assert(MeshTrait<MeshType>::is_mesh(), "Input type is not Mesh");

    if (!mesh.is_components_initialized()) {
        mesh.initialize_components();
    }

    return extract_submeshes(mesh, mesh.get_components(), vertex_mappings, facet_mappings);
}

template <typename MeshType, typename Index>
std::vector<std::unique_ptr<MeshType>> extract_submeshes(
    const MeshType& mesh,
    const std::vector<std::vector<Index>>& facet_groups,
    std::vector<std::vector<Index>>* vertex_mappings = nullptr,
    std::vector<std::vector<Index>>* facet_mappings = nullptr)
{
    static_assert(MeshTrait<MeshType>::is_mesh(), "Input type is not Mesh");

    if (vertex_mappings) {
        vertex_mappings->resize(facet_groups.size());
    }
    if (facet_mappings) {
        facet_mappings->resize(facet_groups.size());
    }

    std::vector<std::unique_ptr<MeshType>> extracted_meshes;
    for (auto i : range(facet_groups.size())) {
        extracted_meshes.emplace_back(extract_submesh(
            mesh,
            facet_groups[i],
            vertex_mappings ? &((*vertex_mappings)[i]) : nullptr,
            facet_mappings ? &((*facet_mappings)[i]) : nullptr));
    }
    return extracted_meshes;
}

template <typename MeshType, typename Index>
std::unique_ptr<MeshType> extract_submesh(
    const MeshType& mesh,
    const std::vector<Index>& selected_facets,
    std::vector<Index>* vertex_mapping = nullptr,
    std::vector<Index>* facet_mapping = nullptr)
{
    static_assert(MeshTrait<MeshType>::is_mesh(), "Input type is not Mesh");

    using MeshIndex = typename MeshType::Index;
    using VertexArray = typename MeshType::VertexArray;
    using FacetArray = typename MeshType::FacetArray;
    const MeshIndex num_selected_facets = safe_cast<MeshIndex>(selected_facets.size());
    const auto vertex_per_facet = mesh.get_vertex_per_facet();

    const auto& vertices = mesh.get_vertices();
    const auto& facets = mesh.get_facets();
    const auto num_facets = mesh.get_num_facets();
    const MeshIndex min_num_vertices = std::min(
        safe_cast<MeshIndex>(num_selected_facets * vertex_per_facet),
        mesh.get_num_vertices());

    FacetArray sub_facets(num_selected_facets, vertex_per_facet);
    std::unordered_map<MeshIndex, MeshIndex> sub_vertex_indices;
    sub_vertex_indices.reserve(min_num_vertices);

    if (facet_mapping) {
        facet_mapping->resize(num_selected_facets);
    }

    MeshIndex count = 0;
    for (auto i : range(num_selected_facets)) {
        la_runtime_assert(
            selected_facets[i] >= 0 && safe_cast<MeshIndex>(selected_facets[i]) < num_facets,
            "Facet index out of bound for submesh extraction");
        for (auto j : range(vertex_per_facet)) {
            const MeshIndex idx = facets(selected_facets[i], j);
            if (sub_vertex_indices.find(idx) == sub_vertex_indices.end()) {
                sub_vertex_indices[idx] = count;
                count++;
            }
            sub_facets(i, j) = sub_vertex_indices[idx];
        }
        if (facet_mapping) {
            (*facet_mapping)[i] = selected_facets[i];
        }
    }

    VertexArray sub_vertices(count, mesh.get_dim());
    if (vertex_mapping) {
        vertex_mapping->resize(count);
    }
    for (const auto& item : sub_vertex_indices) {
        // item.second = index in submesh
        // item.first = index in original mesh
        sub_vertices.row(item.second) = vertices.row(item.first);
        if (vertex_mapping) {
            (*vertex_mapping)[item.second] = item.first;
        }
    }

    return lagrange::create_mesh(std::move(sub_vertices), std::move(sub_facets));
}
} // namespace lagrange
