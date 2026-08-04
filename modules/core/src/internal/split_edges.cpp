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

#include <lagrange/Attribute.h>
#include <lagrange/SurfaceMeshTypes.h>
#include <lagrange/foreach_attribute.h>
#include <lagrange/internal/split_edges.h>
#include <lagrange/internal/split_triangle.h>
#include <lagrange/map_attribute.h>
#include <lagrange/utils/assert.h>
#include <lagrange/utils/invalid.h>
#include <lagrange/utils/span.h>
#include <lagrange/views.h>

#include <algorithm>
#include <array>
#include <vector>

#include <Eigen/Core>

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <tbb/parallel_for.h>
#include <tbb/enumerable_thread_specific.h>
#include <lagrange/utils/warnon.h>
// clang-format on

namespace lagrange::internal {

namespace {

template <typename Scalar, typename Index, typename Derived>
void interpolate_row(
    Eigen::MatrixBase<Derived>& data,
    Index row_to,
    span<Index, 3> rows_from,
    span<Scalar, 3> bc)
{
    la_debug_assert(row_to < static_cast<Index>(data.rows()));
    la_debug_assert(rows_from[0] < static_cast<Index>(data.rows()));
    la_debug_assert(rows_from[1] < static_cast<Index>(data.rows()));
    la_debug_assert(rows_from[2] < static_cast<Index>(data.rows()));
    using ValueType = typename Derived::Scalar;

    if constexpr (std::is_integral_v<ValueType>) {
        data.row(row_to) = (data.row(rows_from[0]).template cast<Scalar>() * bc[0] +
                            data.row(rows_from[1]).template cast<Scalar>() * bc[1] +
                            data.row(rows_from[2]).template cast<Scalar>() * bc[2])
                               .array()
                               .round()
                               .template cast<ValueType>()
                               .eval();
    } else {
        data.row(row_to) = data.row(rows_from[0]) * bc[0] + data.row(rows_from[1]) * bc[1] +
                           data.row(rows_from[2]) * bc[2];
    }
}

template <typename Scalar, typename Index, typename Derived>
void interpolate_row(
    Eigen::MatrixBase<Derived>& data,
    Index row_to,
    Index row_from0,
    Index row_from1,
    Scalar t)
{
    la_debug_assert(row_to < static_cast<Index>(data.rows()));
    la_debug_assert(row_from0 < static_cast<Index>(data.rows()));
    la_debug_assert(row_from1 < static_cast<Index>(data.rows()));
    using ValueType = typename Derived::Scalar;

    if constexpr (std::is_integral_v<ValueType>) {
        data.row(row_to) = (data.row(row_from0).template cast<Scalar>() * t +
                            data.row(row_from1).template cast<Scalar>() * (1 - t))
                               .array()
                               .round()
                               .template cast<ValueType>()
                               .eval();
    } else {
        data.row(row_to) = data.row(row_from0) * t + data.row(row_from1) * (1 - t);
    }
}

template <typename Index>
struct LocalBuffersT
{
    std::vector<Index> chain_buffer;
    std::vector<Index> queue_buffer;
    std::vector<Index> visited_buffer;
};

} // namespace

template <typename Scalar, typename Index>
std::vector<Index> split_edges(
    SurfaceMesh<Scalar, Index>& mesh,
    function_ref<span<Index>(Index)> get_edge_split_pts,
    function_ref<bool(Index)> active_facet)
{
    const Index dim = mesh.get_dimension();
    mesh.initialize_edges();
    const Index num_output_vertices = mesh.get_num_vertices();
    const Index num_input_facets = mesh.get_num_facets();

    std::vector<Index> original_triangle_index;
    std::vector<Index> split_triangles;
    std::vector<size_t> split_triangles_offsets;
    original_triangle_index.reserve(num_input_facets);
    split_triangles.reserve(num_input_facets);
    split_triangles_offsets.reserve(num_input_facets + 1);
    split_triangles_offsets.push_back(0);

    // Compute the number of additional facets, and the CSR layout of split points per split facet.
    std::vector<Index> parent_offsets;
    parent_offsets.reserve(num_input_facets + 1);
    parent_offsets.push_back(0);
    Index num_additional_facets = 0;
    Index num_split_points = 0;
    for (Index fid = 0; fid < num_input_facets; fid++) {
        if (!active_facet(fid)) continue;
        Index chain_size = 3;
        for (Index i = 0; i < 3; i++) {
            Index eid = mesh.get_edge(fid, i);
            la_debug_assert(eid != invalid<Index>());
            Index n = static_cast<Index>(get_edge_split_pts(eid).size());
            chain_size += n;
        }
        if (chain_size == 3) continue;
        num_additional_facets += chain_size - 2;
        num_split_points += chain_size - 3;
        split_triangles_offsets.push_back(num_additional_facets * 3);
        original_triangle_index.push_back(fid);
        parent_offsets.push_back(num_split_points);
    }
    split_triangles.resize(num_additional_facets * 3, invalid<Index>());

    // Parent edge {vertex, end0, end1} per split point, grouped per facet via `parent_offsets` and
    // sorted by vertex so each facet resolves a shared vertex's edge by binary search, not a scan.
    std::vector<std::array<Index, 3>> parent_edges(num_split_points);

    using LocalBuffers = LocalBuffersT<Index>;
    tbb::enumerable_thread_specific<LocalBuffers> local_buffers;
    tbb::parallel_for(size_t(0), original_triangle_index.size(), [&](size_t idx) {
        auto fid = original_triangle_index[idx];
        if (!active_facet(fid)) return;

        auto& buffers = local_buffers.local();
        auto& chain = buffers.chain_buffer;
        chain.clear();
        chain.reserve(64);

        auto corners = mesh.get_facet_vertices(fid);
        Index corner_index[3] = {invalid<Index>(), invalid<Index>(), invalid<Index>()};
        Index parent_cursor = parent_offsets[idx];
        for (Index i = 0; i < 3; i++) {
            Index v0 = corners[i];
            chain.push_back(v0);
            corner_index[i] = static_cast<Index>(chain.size() - 1);

            Index eid = mesh.get_edge(fid, i);
            la_debug_assert(eid != invalid<Index>());
            auto split_pts = get_edge_split_pts(eid);
            auto edge_verts = mesh.get_edge_vertices(eid);
            for (auto vid : split_pts) {
                parent_edges[parent_cursor] = {vid, edge_verts[0], edge_verts[1]};
                ++parent_cursor;
            }
            if (edge_verts[0] == v0) {
                // Edge is oriented in the same direction as the triangle.
                chain.insert(chain.end(), split_pts.begin(), split_pts.end());
            } else {
                // Edge is oriented in the opposite direction.
                la_debug_assert(edge_verts[1] == v0);
                chain.insert(chain.end(), split_pts.rbegin(), split_pts.rend());
            }
        }
        la_debug_assert(parent_cursor == parent_offsets[idx + 1]);
        std::sort(
            parent_edges.begin() + parent_offsets[idx],
            parent_edges.begin() + parent_offsets[idx + 1]);
        la_debug_assert(chain.size() > 3);
        size_t num_sub_triangles = chain.size() - 2;
        size_t curr_size = split_triangles_offsets[idx];
        span<Index> sub_triangulation(split_triangles.data() + curr_size, num_sub_triangles * 3);
        auto& visited = buffers.visited_buffer;
        visited.resize(chain.size() * 3);
        split_triangle<Scalar, Index>(
            num_output_vertices,
            mesh.get_vertex_to_position().get_all(),
            chain,
            visited,
            buffers.queue_buffer,
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

    auto vertices = vertex_view(mesh);
    auto facets = facet_view(mesh);
    auto edge_barycentric = [&](Index v0, Index v1, Index v) {
        Scalar diff = 0;
        Scalar t = 0;
        for (Index i = 0; i < dim; i++) {
            if (vertices(v0, i) == vertices(v1, i)) continue;
            Scalar d = vertices(v0, i) - vertices(v1, i);
            if (std::abs(d) > diff) {
                t = (vertices(v, i) - vertices(v1, i)) / d;
                diff = std::abs(d);
            }
        }
        return t;
    };

    auto barycentric_coordinates =
        [&](Index ori_fid, Index vid, Index p_begin, Index p_end) -> std::array<Scalar, 3> {
        auto vid_0 = facets(ori_fid, 0);
        auto vid_1 = facets(ori_fid, 1);
        auto vid_2 = facets(ori_fid, 2);
        if (vid == vid_0) return {1, 0, 0};
        if (vid == vid_1) return {0, 1, 0};
        if (vid == vid_2) return {0, 0, 1};

        auto it = std::lower_bound(
            parent_edges.begin() + p_begin,
            parent_edges.begin() + p_end,
            vid,
            [](const std::array<Index, 3>& e, Index v) { return e[0] < v; });
        la_debug_assert(it != parent_edges.begin() + p_end && (*it)[0] == vid);
        const Index v0 = (*it)[1];
        const Index v1 = (*it)[2];
        auto t = edge_barycentric(v0, v1, vid);
        if (vid_0 != v0 && vid_0 != v1) {
            if (v0 == vid_1) {
                return {0, t, 1 - t};
            } else {
                return {0, 1 - t, t};
            }
        } else if (vid_1 != v0 && vid_1 != v1) {
            if (v0 == vid_0) {
                return {t, 0, 1 - t};
            } else {
                return {1 - t, 0, t};
            }
        } else {
            la_debug_assert(vid_2 != v0 && vid_2 != v1);
            if (v0 == vid_0) {
                return {t, 1 - t, 0};
            } else {
                return {1 - t, t, 0};
            }
        }
    };

    // Barycentric coordinates of each new corner within its original facet, computed once and
    // reused across every corner/indexed attribute rather than recomputed per attribute.
    std::vector<std::array<Scalar, 3>> corner_bc(static_cast<size_t>(num_additional_facets) * 3);
    tbb::parallel_for(size_t(0), original_triangle_index.size(), [&](size_t i) {
        Index ori_fid = original_triangle_index[i];
        for (size_t j = split_triangles_offsets[i] / 3; j < split_triangles_offsets[i + 1] / 3;
             j++) {
            Index fid = num_input_facets + static_cast<Index>(j);
            for (Index k = 0; k < 3; k++) {
                Index vid = facets(fid, k);
                corner_bc[j * 3 + k] =
                    barycentric_coordinates(ori_fid, vid, parent_offsets[i], parent_offsets[i + 1]);
            }
        }
    });

    auto map_corner_attribute = [&](auto&& data, auto&& corner_to_index) {
        for (size_t i = 0; i < original_triangle_index.size(); i++) {
            Index ori_fid = original_triangle_index[i];
            std::array<Index, 3> ori_corners{
                corner_to_index(ori_fid * 3),
                corner_to_index(ori_fid * 3 + 1),
                corner_to_index(ori_fid * 3 + 2)};
            for (size_t j = split_triangles_offsets[i] / 3; j < split_triangles_offsets[i + 1] / 3;
                 j++) {
                Index fid = num_input_facets + static_cast<Index>(j);

                for (Index k = 0; k < 3; k++) {
                    Index curr_cid = fid * 3 + k;
                    auto& bc = corner_bc[j * 3 + k];
                    interpolate_row<Scalar, Index>(
                        data,
                        corner_to_index(curr_cid),
                        span<Index, 3>(ori_corners.data(), 3),
                        span<Scalar, 3>(bc.data(), 3));
                }
            }
        }
    };

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

    return original_triangle_index;
}

template <typename Scalar, typename Index>
std::vector<Index> split_edges_only(
    SurfaceMesh<Scalar, Index>& mesh,
    function_ref<span<Index>(Index)> get_edge_split_pts,
    function_ref<bool(Index)> active_facet)
{
    const Index dim = mesh.get_dimension();
    mesh.initialize_edges();
    const Index num_input_facets = mesh.get_num_facets();
    const Index num_input_corners = mesh.get_num_corners();

    auto vertices = vertex_view(mesh);
    auto edge_barycentric = [&](Index v0, Index v1, Index v) {
        Scalar diff = 0;
        Scalar t = 0;
        for (Index i = 0; i < dim; i++) {
            if (vertices(v0, i) == vertices(v1, i)) continue;
            Scalar d = vertices(v0, i) - vertices(v1, i);
            if (std::abs(d) > diff) {
                t = (vertices(v, i) - vertices(v1, i)) / d;
                diff = std::abs(d);
            }
        }
        la_runtime_assert(
            diff > 0,
            "Degenerate edge: cannot place a split point on an edge whose endpoints have "
            "identical coordinates");
        return t;
    };

    std::vector<Index> original_facet_index;
    std::vector<Index> new_facet_sizes;
    std::vector<Index> new_facet_indices;
    std::vector<Index> corner_src0;
    std::vector<Index> corner_src1;
    std::vector<Scalar> corner_weight0;

    std::vector<Index> chain;
    for (Index fid = 0; fid < num_input_facets; fid++) {
        if (!active_facet(fid)) continue;

        const Index facet_size = mesh.get_facet_size(fid);
        const Index corner_begin = mesh.get_facet_corner_begin(fid);
        auto corners = mesh.get_facet_vertices(fid);

        chain.clear();
        const size_t chain_start = new_facet_indices.size();
        for (Index i = 0; i < facet_size; i++) {
            const Index v0 = corners[i];
            const Index i_next = (i + 1 == facet_size) ? 0 : i + 1;
            const Index v1 = corners[i_next];
            const Index corner0 = corner_begin + i;
            const Index corner1 = corner_begin + i_next;

            chain.push_back(v0);
            corner_src0.push_back(corner0);
            corner_src1.push_back(invalid<Index>());
            corner_weight0.push_back(1);

            const Index eid = mesh.get_edge(fid, i);
            la_debug_assert(eid != invalid<Index>());
            auto split_pts = get_edge_split_pts(eid);
            const bool same_direction = (mesh.get_edge_vertices(eid)[0] == v0);
            la_debug_assert(same_direction || mesh.get_edge_vertices(eid)[1] == v0);
            for (size_t k = 0; k < split_pts.size(); k++) {
                Index vid = same_direction ? split_pts[k] : split_pts[split_pts.size() - 1 - k];
                chain.push_back(vid);
                corner_src0.push_back(corner0);
                corner_src1.push_back(corner1);
                corner_weight0.push_back(edge_barycentric(v0, v1, vid));
            }
        }

        if (static_cast<Index>(chain.size()) == facet_size) {
            // No split points were introduced on this facet, discard the buffered corner data.
            corner_src0.resize(chain_start);
            corner_src1.resize(chain_start);
            corner_weight0.resize(chain_start);
            continue;
        }

        original_facet_index.push_back(fid);
        new_facet_sizes.push_back(static_cast<Index>(chain.size()));
        new_facet_indices.insert(new_facet_indices.end(), chain.begin(), chain.end());
    }

    if (original_facet_index.empty()) return {};

    // Clear edge data structure since the edge data is costly to maintain,
    // and we are about to change facet connectivity anyway.
    mesh.clear_edges();

    mesh.add_hybrid(
        {new_facet_sizes.data(), new_facet_sizes.size()},
        {new_facet_indices.data(), new_facet_indices.size()});

    // Propagate facet attributes
    par_foreach_named_attribute_write<AttributeElement::Facet>(
        mesh,
        [&](std::string_view name, auto&& attr) {
            if (mesh.attr_name_is_reserved(name)) return;
            auto data = matrix_ref(attr);
            for (size_t i = 0; i < original_facet_index.size(); i++) {
                data.row(num_input_facets + static_cast<Index>(i)) =
                    data.row(original_facet_index[i]);
            }
        });

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
            for (size_t j = 0; j < corner_src0.size(); j++) {
                Index row_to = num_input_corners + static_cast<Index>(j);
                if (corner_src1[j] == invalid<Index>()) {
                    data.row(row_to) = data.row(corner_src0[j]);
                } else {
                    interpolate_row<Scalar, Index>(
                        data,
                        row_to,
                        corner_src0[j],
                        corner_src1[j],
                        corner_weight0[j]);
                }
            }
        });

    // Map back to indexed attributes
    for (auto attr_id : indexed_attribute_ids) {
        map_attribute_in_place(mesh, attr_id, AttributeElement::Indexed);
    }

    return original_facet_index;
}

#define LA_X_split_edges(_, Scalar, Index)                                   \
    template LA_CORE_API std::vector<Index> split_edges<Scalar, Index>(      \
        SurfaceMesh<Scalar, Index>&,                                         \
        function_ref<span<Index>(Index)>,                                    \
        function_ref<bool(Index)>);                                          \
    template LA_CORE_API std::vector<Index> split_edges_only<Scalar, Index>( \
        SurfaceMesh<Scalar, Index>&,                                         \
        function_ref<span<Index>(Index)>,                                    \
        function_ref<bool(Index)>);
LA_SURFACE_MESH_X(split_edges, 0)

} // namespace lagrange::internal
