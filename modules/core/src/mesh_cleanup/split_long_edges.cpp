/*
 * Copyright 2024 Adobe. All rights reserved.
 * This file is licensed to you under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License. You may obtain a copy
 * of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under
 * the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
 * OF ANY KIND, either express or implied. See the License for the specific language
 * governing permissions and limitations under the License.
 */
#include <lagrange/mesh_cleanup/split_long_edges.h>

#include <lagrange/SurfaceMeshTypes.h>
#include <lagrange/cast_attribute.h>
#include <lagrange/compute_edge_lengths.h>
#include <lagrange/foreach_attribute.h>
#include <lagrange/internal/split_edges.h>
#include <lagrange/utils/assert.h>
#include <lagrange/utils/invalid.h>
#include <lagrange/utils/span.h>
#include <lagrange/views.h>

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <tbb/parallel_for.h>
#include <tbb/enumerable_thread_specific.h>
#include <lagrange/utils/warnon.h>
// clang-format on

#include <numeric>

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
void split_long_edges(SurfaceMesh<Scalar, Index>& mesh, SplitLongEdgesOptions options)
{
    la_runtime_assert(mesh.is_triangle_mesh(), "Input mesh is not a triangle mesh.");
    mesh.initialize_edges();

    const Index dim = mesh.get_dimension();
    const Index num_input_vertices = mesh.get_num_vertices();
    const Index num_edges = mesh.get_num_edges();

    constexpr std::string_view internal_active_region_attribute_name =
        "@__active_region_internal__";
    AttributeId internal_active_region_attribute_id = invalid<AttributeId>();
    if (!options.active_region_attribute.empty()) {
        // Cast attribute to a known type for easy access.
        internal_active_region_attribute_id = cast_attribute<uint8_t>(
            mesh,
            options.active_region_attribute,
            internal_active_region_attribute_name);
    }

    // An active facet means its edges are checked against the edge length threshold.
    auto is_active = [&](Index fid) {
        if (internal_active_region_attribute_id == invalid<AttributeId>()) return true;
        auto active_view =
            attribute_vector_view<uint8_t>(mesh, internal_active_region_attribute_id);
        return static_cast<bool>(active_view[fid]);
    };

    // An edge is active if it is adjacent to an active facet.
    auto is_edge_active = [&](Index eid) {
        bool r = false;
        mesh.foreach_facet_around_edge(eid, [&](auto fid) { r |= is_active(fid); });
        return r;
    };

    EdgeLengthOptions edge_length_options;
    edge_length_options.output_attribute_name = options.edge_length_attribute;
    auto edge_length_attr_id = compute_edge_lengths(mesh, edge_length_options);
    auto edge_lengths = attribute_vector_view<Scalar>(mesh, edge_length_attr_id);
    auto input_vertices = vertex_view(mesh);

    // Computer split locations
    Index estimated_num_additional_vertices =
        static_cast<Index>(std::ceil(edge_lengths.sum() / options.max_edge_length));
    std::vector<Scalar> additional_vertices;
    additional_vertices.reserve(estimated_num_additional_vertices * dim);
    std::vector<std::tuple<Index, Index, Scalar>> additional_vertex_sources;
    additional_vertex_sources.reserve(estimated_num_additional_vertices);
    std::vector<Index> edge_split_indices(num_edges + 1);
    edge_split_indices[0] = 0;
    for (Index eid = 0; eid < num_edges; eid++) {
        auto l = edge_lengths[eid];
        if (!is_edge_active(eid) || l <= options.max_edge_length) {
            edge_split_indices[eid + 1] = edge_split_indices[eid];
            continue;
        }

        auto n = static_cast<Index>(std::ceil(l / options.max_edge_length));
        assert(n > 1);

        auto [v0, v1] = mesh.get_edge_vertices(eid);
        auto p0 = input_vertices.row(v0).eval();
        auto p1 = input_vertices.row(v1).eval();
        for (Index i = 1; i < n; i++) {
            Scalar t = static_cast<Scalar>(i) / n;
            auto p = ((1 - t) * p0 + t * p1).eval();
            additional_vertices.insert(additional_vertices.end(), p.data(), p.data() + dim);
            additional_vertex_sources.emplace_back(v0, v1, t);
        }

        edge_split_indices[eid + 1] = edge_split_indices[eid] + n - 1;
    };

    if (internal_active_region_attribute_id != invalid<AttributeId>()) {
        // Remove the internal active region attribute
        mesh.delete_attribute(internal_active_region_attribute_name);
    }

    if (additional_vertices.empty()) return;

    // Update vertices
    const Index num_additional_vertices = static_cast<Index>(additional_vertices.size()) / dim;
    mesh.add_vertices(
        num_additional_vertices,
        {additional_vertices.data(), additional_vertices.size()});

    // Interpolate vertex attributes for newly added vertices
    par_foreach_named_attribute_write<AttributeElement::Vertex>(
        mesh,
        [&](std::string_view name, auto&& attr) {
            using ValueType = typename std::decay_t<decltype(attr)>::ValueType;
            if (mesh.attr_name_is_reserved(name)) return;
            auto data = matrix_ref(attr);
            static_assert(std::is_same_v<ValueType, typename std::decay_t<decltype(data)>::Scalar>);
            for (Index i = 0; i < num_additional_vertices; i++) {
                Index v0, v1;
                Scalar t;
                std::tie(v0, v1, t) = additional_vertex_sources[i];
                interpolate_row(data, num_input_vertices + i, v0, v1, t);
            }
        });

    std::vector<Index> edge_split_pts(num_additional_vertices);
    std::iota(edge_split_pts.begin(), edge_split_pts.end(), num_input_vertices);

    auto facets_to_remove = internal::split_edges(
        mesh,
        function_ref<span<Index>(Index)>([&](Index eid) -> span<Index> {
            Index n = edge_split_indices[eid + 1] - edge_split_indices[eid];
            return span<Index>(edge_split_pts.data() + edge_split_indices[eid], n);
        }),
        function_ref<bool(Index)>([](Index) { return true; }));
    mesh.remove_facets(facets_to_remove);

    if (options.recursive) {
        split_long_edges(mesh, options);
    }
}

#define LA_X_split_long_edges(_, Scalar, Index)                \
    template LA_CORE_API void split_long_edges<Scalar, Index>( \
        SurfaceMesh<Scalar, Index>&,                           \
        SplitLongEdgesOptions);
LA_SURFACE_MESH_X(split_long_edges, 0)
} // namespace lagrange
