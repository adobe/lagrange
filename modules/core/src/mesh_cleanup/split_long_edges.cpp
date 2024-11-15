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
#include <lagrange/map_attribute.h>
#include <lagrange/utils/assert.h>
#include <lagrange/utils/invalid.h>
#include <lagrange/views.h>

#include "split_triangle.h"

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <tbb/parallel_for.h>
#include <tbb/enumerable_thread_specific.h>
#include <lagrange/utils/warnon.h>
// clang-format on

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
    const Index num_input_facets = mesh.get_num_facets();

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

    // A facet may be involved in splitting if one of its edges is active.
    auto is_facet_involved = [&](Index fid) {
        auto e0 = mesh.get_edge(fid, 0);
        auto e1 = mesh.get_edge(fid, 1);
        auto e2 = mesh.get_edge(fid, 2);
        return is_edge_active(e0) || is_edge_active(e1) || is_edge_active(e2);
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
    if (additional_vertices.empty()) return;

    // Update vertices
    const Index num_additional_vertices = static_cast<Index>(additional_vertices.size()) / dim;
    mesh.add_vertices(
        num_additional_vertices,
        {additional_vertices.data(), additional_vertices.size()});
    auto output_vertices = vertex_view(mesh);

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

    // Split triangles
    std::vector<Index> original_triangle_index;
    std::vector<Index> split_triangles;
    std::vector<size_t> split_triangles_offsets;
    original_triangle_index.reserve(num_input_facets);
    split_triangles.reserve(num_input_facets);
    split_triangles_offsets.reserve(num_input_facets + 1);
    split_triangles_offsets.push_back(0);

    // Compute the number of additional facets
    Index num_additional_facets = 0;
    for (Index fid = 0; fid < num_input_facets; fid++) {
        if (!is_facet_involved(fid)) continue;
        Index chain_size = 3;
        for (Index i = 0; i < 3; i++) {
            Index eid = mesh.get_edge(fid, i);
            la_debug_assert(eid != invalid<Index>());
            Index n = edge_split_indices[eid + 1] - edge_split_indices[eid];
            chain_size += n;
        }
        if (chain_size == 3) continue;
        num_additional_facets += chain_size - 2;
        split_triangles_offsets.push_back(num_additional_facets * 3);
        original_triangle_index.push_back(fid);
    }
    split_triangles.resize(num_additional_facets * 3, invalid<Index>());

    tbb::enumerable_thread_specific<std::vector<Index>> chain_buffer;
    tbb::enumerable_thread_specific<std::vector<Index>> queue_buffer;
    tbb::enumerable_thread_specific<std::vector<Index>> visited_buffer;
    tbb::parallel_for(size_t(0), original_triangle_index.size(), [&](size_t idx) {
        auto fid = original_triangle_index[idx];
        auto& chain = chain_buffer.local();
        chain.clear();
        chain.reserve(64);

        auto corners = mesh.get_facet_vertices(fid);
        Index corner_index[3] = {invalid<Index>(), invalid<Index>(), invalid<Index>()};
        for (Index i = 0; i < 3; i++) {
            Index v0 = corners[i];
            chain.push_back(v0);
            corner_index[i] = static_cast<Index>(chain.size() - 1);

            Index eid = mesh.get_edge(fid, i);
            la_debug_assert(eid != invalid<Index>());
            Index n = edge_split_indices[eid + 1] - edge_split_indices[eid];
            if (mesh.get_edge_vertices(eid)[0] == v0) {
                // Edge is oriented in the same direction as the triangle.
                for (Index j = 0; j < n; j++) {
                    chain.push_back(num_input_vertices + edge_split_indices[eid] + j);
                }
            } else {
                // Edge is oriented in the opposite direction.
                la_debug_assert(mesh.get_edge_vertices(eid)[1] == v0);
                for (Index j = 0; j < n; j++) {
                    chain.push_back(num_input_vertices + edge_split_indices[eid] + n - 1 - j);
                }
            }
        }
        la_debug_assert(chain.size() > 3);
        size_t num_sub_triangles = chain.size() - 2;
        size_t curr_size = split_triangles_offsets[idx];
        span<Index> sub_triangulation(split_triangles.data() + curr_size, num_sub_triangles * 3);
        auto& visited = visited_buffer.local();
        visited.resize(chain.size() * 3);
        split_triangle<Scalar, Index>(
            static_cast<Index>(output_vertices.rows()),
            span<const Scalar>(output_vertices.data(), output_vertices.size()),
            chain,
            visited,
            queue_buffer.local(),
            corner_index[0],
            corner_index[1],
            corner_index[2],
            sub_triangulation);
    });

    // Clear edge data structure since the edge data is costly to maintain,
    // and we will be updating triangulation anyway.
    mesh.clear_edges();

    la_debug_assert(split_triangles.size() % 3 == 0);
    mesh.add_triangles(num_additional_facets, {split_triangles.data(), split_triangles.size()});

    // Propagate facet attributes
    par_foreach_named_attribute_write<AttributeElement::Facet>(
        mesh,
        [&](std::string_view name, auto&& attr) {
            if (mesh.attr_name_is_reserved(name)) return;
            auto data = matrix_ref(attr);
            for (size_t i = 0; i < original_triangle_index.size(); i++) {
                for (size_t j = split_triangles_offsets[i] / 3;
                     j < split_triangles_offsets[i + 1] / 3;
                     j++) {
                    data.row(num_input_facets + static_cast<Index>(j)) =
                        data.row(original_triangle_index[i]);
                }
            }
        });

    auto map_corner_attribute = [&](auto&& data, auto&& corner_to_index) {
        auto facets = facet_view(mesh);
        for (size_t i = 0; i < original_triangle_index.size(); i++) {
            for (size_t j = split_triangles_offsets[i] / 3; j < split_triangles_offsets[i + 1] / 3;
                 j++) {
                Index fid = num_input_facets + static_cast<Index>(j);

                for (Index k = 0; k < 3; k++) {
                    Index curr_cid = fid * 3 + k;
                    Index vid = facets(fid, k);
                    if (vid < num_input_vertices) {
                        // Corner comes from a corner in the original triangulation.
                        Index ori_cid = invalid<Index>();
                        for (Index l = 0; l < 3; l++) {
                            if (vid == facets(original_triangle_index[i], l)) {
                                ori_cid = original_triangle_index[i] * 3 + l;
                            }
                        }
                        la_debug_assert(ori_cid != invalid<Index>());
                        data.row(corner_to_index(curr_cid)) = data.row(corner_to_index(ori_cid));
                    } else {
                        // Corner comes from a split edge.
                        auto [v0, v1, t] = additional_vertex_sources[vid - num_input_vertices];

                        Index ori_c0 = invalid<Index>();
                        Index ori_c1 = invalid<Index>();
                        for (Index l = 0; l < 3; l++) {
                            if (v0 == facets(original_triangle_index[i], l)) {
                                ori_c0 = original_triangle_index[i] * 3 + l;
                            }
                            if (v1 == facets(original_triangle_index[i], l)) {
                                ori_c1 = original_triangle_index[i] * 3 + l;
                            }
                        }
                        la_debug_assert(ori_c0 != invalid<Index>());
                        la_debug_assert(ori_c1 != invalid<Index>());
                        interpolate_row(
                            data,
                            corner_to_index(curr_cid),
                            corner_to_index(ori_c0),
                            corner_to_index(ori_c1),
                            t);
                    }
                }
            }
        }
    };

    if (internal_active_region_attribute_id != invalid<AttributeId>()) {
        // Remove the internal active region attribute
        mesh.delete_attribute(internal_active_region_attribute_name);
    }

    // Convert indexed attributes to corner attributes
    std::vector<AttributeId> indexed_attribute_ids;
    seq_foreach_named_attribute_read<AttributeElement::Indexed>(
        mesh,
        [&](std::string_view name, [[maybe_unused]] auto&& attr) {
            if (mesh.attr_name_is_reserved(name)) return;
            indexed_attribute_ids.push_back(mesh.get_attribute_id(name));
        });
    for (auto& attr_id : indexed_attribute_ids) {
        attr_id = map_attribute_in_place(mesh, attr_id, AttributeElement::Corner);
    }

    // Propagate corner attributes
    par_foreach_named_attribute_write<AttributeElement::Corner>(
        mesh,
        [&](std::string_view name, auto&& attr) {
            if (mesh.attr_name_is_reserved(name)) return;
            auto data = matrix_ref(attr);
            map_corner_attribute(data, [&](Index cid) { return cid; });
        });

    // Map back to indexed attributes
    for (auto attr_id : indexed_attribute_ids) {
        map_attribute_in_place(mesh, attr_id, AttributeElement::Indexed);
    }

    mesh.remove_facets(original_triangle_index);

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
