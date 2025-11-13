/*
 * Copyright 2025 Adobe. All rights reserved.
 * This file is licensed to you under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License. You may obtain a copy
 * of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under
 * the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
 * OF ANY KIND, either express or implied. See the License for the specific language
 * governing permissions and limitations under the License.
 */

#include <lagrange/SurfaceMeshTypes.h>
#include <lagrange/foreach_attribute.h>
#include <lagrange/internal/split_edges.h>
#include <lagrange/split_facets_by_material.h>
#include <lagrange/utils/assert.h>
#include <lagrange/utils/invalid.h>
#include <lagrange/views.h>

#include <algorithm>
#include <vector>

namespace lagrange {

namespace {

template <typename Derived, typename Scalar, typename Index>
void interpolate_row(
    Eigen::MatrixBase<Derived>& data,
    Index row_to,
    Index row_from_1,
    Index row_from_2,
    Scalar t)
{
    la_debug_assert(row_to < static_cast<Index>(data.rows()));
    la_debug_assert(row_from_1 < static_cast<Index>(data.rows()));
    la_debug_assert(row_from_2 < static_cast<Index>(data.rows()));
    using ValueType = typename Derived::Scalar;

    if constexpr (std::is_integral_v<ValueType>) {
        data.row(row_to) = (data.row(row_from_1).template cast<Scalar>() * (1 - t) +
                            data.row(row_from_2).template cast<Scalar>() * t)
                               .array()
                               .round()
                               .template cast<ValueType>()
                               .eval();
    } else {
        data.row(row_to) = data.row(row_from_1) * (1 - t) + data.row(row_from_2) * t;
    }
}

} // namespace


template <typename Scalar, typename Index>
void split_facets_by_material(
    SurfaceMesh<Scalar, Index>& mesh,
    std::string_view material_attribute_name)
{
    const Index dim = mesh.get_dimension();
    la_runtime_assert(
        mesh.has_attribute(material_attribute_name),
        "Mesh does not have segment attribute");
    auto segment_labels = attribute_matrix_view<Scalar>(mesh, material_attribute_name);
    Index num_segments = static_cast<Index>(segment_labels.cols());

    auto vertices = vertex_view(mesh);
    mesh.initialize_edges();

    const Index num_vertices = mesh.get_num_vertices();
    const Index num_edges = mesh.get_num_edges();
    Index num_new_vertices = 0;

    // Create a mapping from edges to new vertices
    std::vector<Index> edge_to_new_vertex(num_edges, invalid<Index>());
    std::vector<Scalar> edge_split_ratios(num_edges, invalid<Scalar>());
    std::vector<Scalar> new_vertices;
    new_vertices.reserve(num_edges * dim / 20); // Reserve space for new vertices
    for (Index ei = 0; ei < num_edges; ei++) {
        auto [v0, v1] = mesh.get_edge_vertices(ei);
        span<const Scalar> labels0{
            segment_labels.data() + v0 * num_segments,
            static_cast<size_t>(num_segments)};
        span<const Scalar> labels1{
            segment_labels.data() + v1 * num_segments,
            static_cast<size_t>(num_segments)};

        auto max_itr_0 = std::max_element(labels0.begin(), labels0.end());
        auto max_itr_1 = std::max_element(labels1.begin(), labels1.end());

        Index label_0 = static_cast<Index>(max_itr_0 - labels0.begin());
        Index label_1 = static_cast<Index>(max_itr_1 - labels1.begin());

        if (label_0 == label_1) {
            // No segment boundary to insert
            continue;
        }

        Scalar v0_l0 = labels0[label_0];
        Scalar v0_l1 = labels0[label_1];
        Scalar v1_l0 = labels1[label_0];
        Scalar v1_l1 = labels1[label_1];

        Scalar t = (v0_l0 - v0_l1) / (v0_l0 - v0_l1 + v1_l1 - v1_l0);
        la_debug_assert(t >= 0 && t <= 1);
        if (dim == 3) {
            Eigen::Matrix<Scalar, 1, 3> pos = (1 - t) * vertices.row(v0) + t * vertices.row(v1);
            new_vertices.insert(new_vertices.end(), pos.data(), pos.data() + 3);
        } else {
            la_debug_assert(dim == 2);
            Eigen::Matrix<Scalar, 1, 2> pos = (1 - t) * vertices.row(v0) + t * vertices.row(v1);
            new_vertices.insert(new_vertices.end(), pos.data(), pos.data() + 2);
        }
        edge_split_ratios[ei] = t;
        edge_to_new_vertex[ei] = num_vertices + num_new_vertices;
        num_new_vertices++;
    }
    mesh.add_vertices(num_new_vertices, {new_vertices.data(), new_vertices.size()});

    // Interpolate vertex attributes for newly added vertices
    par_foreach_named_attribute_write<AttributeElement::Vertex>(
        mesh,
        [&](std::string_view name, auto&& attr) {
            using ValueType = typename std::decay_t<decltype(attr)>::ValueType;
            if (mesh.attr_name_is_reserved(name)) return;
            auto data = matrix_ref(attr);
            static_assert(std::is_same_v<ValueType, typename std::decay_t<decltype(data)>::Scalar>);
            for (Index ei = 0; ei < num_edges; ei++) {
                if (edge_to_new_vertex[ei] == invalid<Index>()) {
                    continue; // No new vertex for this edge
                }

                auto [v0, v1] = mesh.get_edge_vertices(ei);
                Scalar t = edge_split_ratios[ei];
                Index vi = edge_to_new_vertex[ei];
                la_debug_assert(t != invalid<Scalar>());
                la_debug_assert(t >= 0 && t <= 1);

                interpolate_row(data, vi, v0, v1, t);
            }
        });

    // Split edges based on the new vertices
    auto facets_to_remove = internal::split_edges(
        mesh,
        function_ref<span<Index>(Index)>([&](Index ei) -> span<Index> {
            if (edge_to_new_vertex[ei] != invalid<Index>()) {
                la_debug_assert(edge_to_new_vertex[ei] < mesh.get_num_vertices());
                return {&edge_to_new_vertex[ei], 1};
            } else {
                return {};
            }
        }),
        function_ref<bool(Index)>([&]([[maybe_unused]] Index fi) -> bool { return true; }));
    mesh.remove_facets(facets_to_remove);
}

#define LA_X_split_facets_by_material(_, Scalar, Index) \
    template LA_CORE_API void split_facets_by_material( \
        SurfaceMesh<Scalar, Index>&,                    \
        std::string_view);
LA_SURFACE_MESH_X(split_facets_by_material, 0)

} // namespace lagrange
