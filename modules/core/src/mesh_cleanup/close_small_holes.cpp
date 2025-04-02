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
#include <lagrange/extract_boundary_loops.h>
#include <lagrange/foreach_attribute.h>
#include <lagrange/map_attribute.h>
#include <lagrange/mesh_cleanup/close_small_holes.h>
#include <lagrange/triangulate_polygonal_facets.h>
#include <lagrange/utils/invalid.h>
#include <lagrange/views.h>

#include <algorithm>

namespace lagrange {

namespace {

/**
 * Processes vertex attributes for the given mesh.
 *
 * This function iterates over all vertex attributes of the mesh and updates the attributes for
 * newly added vertices. The new vertex attributes are computed as the average of the attributes of
 * the adjacent vertices in the one-ring neighborhood.
 *
 * @tparam Scalar The scalar type used for the mesh.
 * @tparam Index The index type used for the mesh.
 *
 * @param mesh The surface mesh to process.
 * @param input_num_vertices The number of vertices in the input mesh before adding new vertices.
 * @param num_vertices The total number of vertices in the mesh after adding new vertices.
 */
template <typename Scalar, typename Index>
void process_vertex_attributes(
    SurfaceMesh<Scalar, Index>& mesh,
    Index input_num_vertices,
    Index num_vertices)
{
    par_foreach_named_attribute_write<AttributeElement::Vertex>(
        mesh,
        [&](std::string_view name, auto&& attr) {
            using AttributeType = std::decay_t<decltype(attr)>;
            using ValueType = typename AttributeType::ValueType;

            if (mesh.attr_name_is_reserved(name)) return;
            auto values = matrix_ref(attr);

            for (Index vid = input_num_vertices; vid < num_vertices; vid++) {
                size_t one_ring_size = 0;
                mesh.foreach_corner_around_vertex(vid, [&](Index cid) {
                    Index cid_next = mesh.get_next_corner_around_facet(cid);
                    Index vid_adj = mesh.get_corner_vertex(cid_next);
                    values.row(vid) += values.row(vid_adj);
                    one_ring_size++;
                });
                if (one_ring_size > 0) {
                    values.row(vid) /= static_cast<ValueType>(one_ring_size);
                }
            }
        });
}

/**
 * Processes facet attributes for the given mesh.
 *
 * This function iterates over all facet attributes of the mesh and updates the attributes for newly
 * added facets. The new facet attributes are computed as the average of the attributes of the
 * adjacent facets.
 *
 * @tparam Scalar The scalar type used for the mesh.
 * @tparam Index The index type used for the mesh.
 *
 * @param mesh The surface mesh to process.
 * @param input_num_facets The number of facets in the input mesh before adding new facets.
 * @param num_facets The total number of facets in the mesh after adding new facets.
 */
template <typename Scalar, typename Index>
void process_facet_attributes(
    SurfaceMesh<Scalar, Index>& mesh,
    Index input_num_facets,
    Index num_facets)
{
    par_foreach_named_attribute_write<AttributeElement::Facet>(
        mesh,
        [&](std::string_view name, auto&& attr) {
            using AttributeType = std::decay_t<decltype(attr)>;
            using ValueType = typename AttributeType::ValueType;

            if (mesh.attr_name_is_reserved(name)) return;
            auto values = matrix_ref(attr);

            for (Index fid = input_num_facets; fid < num_facets; fid++) {
                ValueType normalize_factor = 0;
                for (Index cid = mesh.get_facet_corner_begin(fid);
                     cid < mesh.get_facet_corner_end(fid);
                     cid++) {
                    Index cid_next = mesh.get_counterclockwise_corner_around_vertex(cid);
                    Index cid_prev = mesh.get_clockwise_corner_around_vertex(cid);

                    if (cid_next != invalid<Index>()) {
                        Index fid_next = mesh.get_corner_facet(cid_next);
                        if (fid_next < input_num_facets) {
                            values.row(fid) += values.row(fid_next);
                            normalize_factor += 1;
                        }
                    }
                    if (cid_prev != invalid<Index>()) {
                        Index fid_prev = mesh.get_corner_facet(cid_prev);
                        if (fid_prev < input_num_facets) {
                            values.row(fid) += values.row(fid_prev);
                            normalize_factor += 1;
                        }
                    }
                }
                if (normalize_factor != 0) {
                    values.row(fid) /= normalize_factor;
                }
            }
        });
}

/**
 * Processes corner attributes for the given mesh.
 *
 * This function iterates over all corner attributes of the mesh and updates the attributes for
 * newly added corners. The new corner attributes are computed as the average of the attributes of
 * the adjacent corners.
 *
 * @tparam Scalar The scalar type used for the mesh.
 * @tparam Index The index type used for the mesh.
 *
 * @param mesh The surface mesh to process.
 * @param input_num_vertices The number of vertices in the input mesh before adding new vertices.
 * @param input_num_facets The number of facets in the input mesh before adding new facets.
 * @param num_facets The total number of facets in the mesh after adding new facets.
 */
template <typename Scalar, typename Index>
void process_corner_attributes(
    SurfaceMesh<Scalar, Index>& mesh,
    Index input_num_vertices,
    Index input_num_facets,
    Index input_num_corners,
    Index num_facets)
{
    par_foreach_named_attribute_write<AttributeElement::Corner>(
        mesh,
        [&](std::string_view name, auto&& attr) {
            using AttributeType = std::decay_t<decltype(attr)>;
            using ValueType = typename AttributeType::ValueType;

            if (mesh.attr_name_is_reserved(name)) return;
            auto values = matrix_ref(attr);

            for (Index fid = input_num_facets; fid < num_facets; fid++) {
                Index barycenter_cid = invalid<Index>();
                for (Index cid = mesh.get_facet_corner_begin(fid);
                     cid < mesh.get_facet_corner_end(fid);
                     cid++) {
                    Index vid = mesh.get_corner_vertex(cid);
                    if (vid >= input_num_vertices) {
                        // Barycenter corners will be handled differently.
                        la_debug_assert(barycenter_cid == invalid<Index>());
                        barycenter_cid = cid;
                        continue;
                    }
                    Index cid_next = mesh.get_counterclockwise_corner_around_vertex(cid);
                    Index cid_prev = mesh.get_clockwise_corner_around_vertex(cid);

                    values.row(cid).setZero();

                    ValueType normalize_factor = 0;
                    if (cid_next != invalid<Index>()) {
                        if (cid_next < input_num_corners) {
                            values.row(cid) += values.row(cid_next);
                            normalize_factor += 1;
                        }
                    }
                    if (cid_prev != invalid<Index>()) {
                        if (cid_prev < input_num_corners) {
                            values.row(cid) += values.row(cid_prev);
                            normalize_factor += 1;
                        }
                    }
                    if (normalize_factor != 0) {
                        values.row(cid) /= normalize_factor;
                    }
                }
                if (barycenter_cid != invalid<Index>()) {
                    Index facet_size = mesh.get_facet_size(fid);
                    la_debug_assert(facet_size == 3);
                    Index cid_begin = mesh.get_facet_corner_begin(fid);
                    Index cid_next = (barycenter_cid + 1 - cid_begin) % facet_size + cid_begin;
                    Index cid_prev =
                        (barycenter_cid + facet_size - cid_begin - 1) % facet_size + cid_begin;
                    la_debug_assert(mesh.get_corner_vertex(cid_next) < input_num_vertices);
                    la_debug_assert(mesh.get_corner_vertex(cid_prev) < input_num_vertices);
                    values.row(barycenter_cid) =
                        (values.row(cid_next) + values.row(cid_prev)) / static_cast<ValueType>(2);
                }
            }
        });
}

/**
 * Processes edge attributes of a surface mesh.
 *
 * This function iterates over the edge attributes of the given surface mesh and processes them.
 * It sets the attribute values of spoke edges (edge connecting a hole boundary vertex to hole
 * barycenter) to be the average value of neighboring non-spoke edges.
 *
 * @tparam Scalar The scalar type used in the surface mesh.
 * @tparam Index The index type used in the surface mesh.
 *
 * @param mesh The surface mesh to process.
 * @param input_num_facets The number of input facets in the mesh.
 */
template <typename Scalar, typename Index>
void process_edge_attributes(SurfaceMesh<Scalar, Index>& mesh, Index input_num_facets)
{
    par_foreach_named_attribute_write<AttributeElement::Edge>(
        mesh,
        [&](std::string_view name, auto&& attr) {
            using AttributeType = std::decay_t<decltype(attr)>;
            using ValueType = typename AttributeType::ValueType;
            if (mesh.attr_name_is_reserved(name)) return;

            auto is_spoke_edge = [&](Index eid) {
                bool is_spoke = true;
                mesh.foreach_facet_around_edge(eid, [&](Index fid) {
                    if (fid < input_num_facets) is_spoke = false;
                });
                return is_spoke;
            };

            const auto num_edges = mesh.get_num_edges();
            auto values = matrix_ref(attr);
            for (Index eid = 0; eid < num_edges; eid++) {
                if (!is_spoke_edge(eid)) continue;
                values.row(eid).setZero();

                Index cid = mesh.get_first_corner_around_edge(eid);
                Index nomralize_factor = 0;
                while (cid != invalid<Index>()) {
                    Index cid_next = mesh.get_next_corner_around_facet(cid);
                    Index cid_prev = mesh.get_next_corner_around_facet(cid_next);
                    la_debug_assert(cid == mesh.get_next_corner_around_facet(cid_prev));

                    Index eid_next = mesh.get_corner_edge(cid_next);
                    Index eid_prev = mesh.get_corner_edge(cid_prev);

                    if (!is_spoke_edge(eid_next)) {
                        values.row(eid) += values.row(eid_next);
                        nomralize_factor++;
                    }
                    if (!is_spoke_edge(eid_prev)) {
                        values.row(eid) += values.row(eid_prev);
                        nomralize_factor++;
                    }

                    cid = mesh.get_next_corner_around_edge(cid);
                }

                if (nomralize_factor > 0) {
                    values.row(eid) /= static_cast<ValueType>(nomralize_factor);
                }
            }
        });
}

/**
 * Processes indexed attributes for the given mesh.
 *
 * This function iterates over all indexed attributes of the mesh and updates the attributes for
 * newly added corners. Corner on the hole boundary will get the same index value as the adjacent
 * corner. Barycenter corners will be clustered based on seams, and each cluster will get a value
 * that is average of the values on the hole boundary cut by seams.
 *
 * @tparam Scalar The scalar type used for the mesh.
 * @tparam Index The index type used for the mesh.
 *
 * @param mesh The surface mesh to process.
 * @param input_num_vertices The number of vertices in the input mesh before adding new vertices.
 * @param input_num_facets The number of facets in the input mesh before adding new facets.
 * @param num_facets The total number of facets in the mesh after adding new facets.
 */
template <typename Scalar, typename Index>
void process_indexed_attributes(
    SurfaceMesh<Scalar, Index>& mesh,
    Index input_num_vertices,
    Index input_num_facets,
    Index num_facets)
{
    par_foreach_named_attribute_write<
        AttributeElement::Indexed>(mesh, [&](std::string_view name, auto&& attr) {
        using AttributeType = std::decay_t<decltype(attr)>;
        using ValueType = typename AttributeType::ValueType;

        if (mesh.attr_name_is_reserved(name)) return;

        auto& value_attr = attr.values();
        auto& index_attr = attr.indices();
        auto values = matrix_ref(value_attr);
        const size_t num_channels = value_attr.get_num_channels();
        const size_t num_elements = value_attr.get_num_elements();

        std::vector<bool> contains_barycenter(num_facets - input_num_facets, false);
        for (Index fid = input_num_facets; fid < num_facets; fid++) {
            // Check whether the facet contains a hole barycenter vertex.
            for (Index cid = mesh.get_facet_corner_begin(fid); cid < mesh.get_facet_corner_end(fid);
                 cid++) {
                Index vid = mesh.get_corner_vertex(cid);
                if (vid >= input_num_vertices) {
                    contains_barycenter[fid - input_num_facets] = true;
                    break;
                }
            }

            // Propagate attribute values for boundaries of the hole facet.
            // We will reuse the values from adjacent corners.
            for (Index cid = mesh.get_facet_corner_begin(fid); cid < mesh.get_facet_corner_end(fid);
                 cid++) {
                Index vid = mesh.get_corner_vertex(cid);
                if (vid >= input_num_vertices) continue;

                Index cid_next = mesh.get_counterclockwise_corner_around_vertex(cid);
                Index cid_prev = mesh.get_clockwise_corner_around_vertex(cid);
                la_debug_assert(!(cid_next == invalid<Index>() && cid_prev == invalid<Index>()));

                if (cid_next != invalid<Index>()) {
                    auto fid_next = mesh.get_corner_facet(cid_next);
                    if (fid_next < input_num_facets) {
                        index_attr.ref(cid) = index_attr.get(cid_next);
                    }
                }
                if (cid_prev != invalid<Index>()) {
                    auto fid_prev = mesh.get_corner_facet(cid_prev);
                    if (fid_prev < input_num_facets) {
                        index_attr.ref(cid) = index_attr.get(cid_prev);
                    }
                }
            }
        }

        std::vector<bool> visited(num_facets - input_num_facets, false);
        auto get_next_corner_around_vertex_without_crossing_seams = [&](Index cid) {
            // clang-format off
            //
            //         ╱|╲
            //        ╱D|A╲
            //       ╱  |  ╲
            //      ╱   |   ╲
            //     ╱___C|B___╲
            //
            //    A: cid
            //    B: cid_next
            //    C: cid_adj_prev
            //    D: cid_adj
            //
            // clang-format on
            Index eid = mesh.get_corner_edge(cid);
            Index c_next = mesh.get_next_corner_around_facet(cid);
            Index c_adj_prev = mesh.get_next_corner_around_edge(cid);
            if (c_adj_prev == invalid<Index>()) {
                c_adj_prev = mesh.get_first_corner_around_edge(eid);
            }
            la_debug_assert(mesh.get_corner_vertex(c_next) == mesh.get_corner_vertex(c_adj_prev));
            if (index_attr.get(c_next) == index_attr.get(c_adj_prev)) {
                Index c_adj = mesh.get_next_corner_around_facet(c_adj_prev);
                return c_adj;
            } else {
                // Seam crossing detected.
                return invalid<Index>();
            }
        };

        auto get_prev_corner_around_vertex_without_crossing_seams = [&](Index cid) {
            // clang-format off
            //
            //         ╱|╲
            //        ╱D|A╲
            //       ╱  |  ╲
            //      ╱   |   ╲
            //     ╱___C|B___╲
            //
            //    A: cid_adj
            //    B: cid_adj_next
            //    C: cid_prev
            //    D: cid
            //
            // clang-format on
            Index fid = mesh.get_corner_facet(cid);
            Index c_begin = mesh.get_facet_corner_begin(fid);
            Index c_end = mesh.get_facet_corner_end(fid);
            Index c_prev = cid + 2;
            if (c_prev >= c_end) c_prev = c_begin + (c_prev - c_end);
            Index c_adj = mesh.get_next_corner_around_edge(c_prev);
            if (c_adj == invalid<Index>()) {
                c_adj = mesh.get_first_corner_around_edge(mesh.get_corner_edge(c_prev));
            }
            Index c_adj_next = mesh.get_next_corner_around_facet(c_adj);
            la_debug_assert(mesh.get_corner_vertex(c_prev) == mesh.get_corner_vertex(c_adj_next));
            if (index_attr.get(c_prev) == index_attr.get(c_adj_next)) {
                return c_adj;
            } else {
                // Seam crossing detected.
                return invalid<Index>();
            }
        };

        std::vector<ValueType> additional_values;
        additional_values.reserve((num_facets - input_num_facets) * 3);
        std::vector<bool> value_used(num_elements, false);

        auto process_barycenter_corner = [&](Index cid, auto& val) {
            Index fid = mesh.get_corner_facet(cid);
            la_debug_assert(fid >= input_num_facets);
            la_debug_assert(!visited[fid - input_num_facets]);
            visited[fid - input_num_facets] = true;

            Index c_next = mesh.get_next_corner_around_facet(cid);
            Index c_prev = mesh.get_next_corner_around_facet(c_next);
            la_debug_assert(c_next != cid);
            la_debug_assert(c_prev != cid);
            la_debug_assert(c_prev != c_next);

            Index i_prev = index_attr.get(c_prev);
            Index i_next = index_attr.get(c_next);
            size_t count = 0;
            if (!value_used[i_prev]) {
                val += values.row(i_prev);
                value_used[i_prev] = true;
                count++;
            }
            if (!value_used[i_next]) {
                val += values.row(i_next);
                value_used[i_next] = true;
                count++;
            }

            const Index id = static_cast<Index>(num_elements) +
                             static_cast<Index>(additional_values.size() / num_channels) - 1;
            index_attr.ref(cid) = id;
            return count;
        };

        auto flood_without_crossing_seams = [&](Index cid) {
            additional_values.insert(additional_values.end(), num_channels, 0);
            Eigen::Map<Eigen::Matrix<ValueType, 1, -1>> val(
                additional_values.data() + additional_values.size() - num_channels,
                num_channels);

            value_used.assign(num_elements, false);

            size_t count = process_barycenter_corner(cid, val);

            Index c_next = get_next_corner_around_vertex_without_crossing_seams(cid);
            while (c_next != invalid<Index>()) {
                count += process_barycenter_corner(c_next, val);
                c_next = get_next_corner_around_vertex_without_crossing_seams(c_next);
            }
            Index c_prev = get_prev_corner_around_vertex_without_crossing_seams(cid);
            while (c_prev != invalid<Index>()) {
                count += process_barycenter_corner(c_prev, val);
                c_prev = get_prev_corner_around_vertex_without_crossing_seams(c_prev);
            }

            la_debug_assert(count > 0);
            val /= static_cast<ValueType>(count);
        };

        for (Index fid = input_num_facets; fid < num_facets; fid++) {
            if (!contains_barycenter[fid - input_num_facets]) {
                visited[fid - input_num_facets] = true;
                continue;
            }

            if (visited[fid - input_num_facets]) continue;

            // Compute values for the barycenter corners.
            for (Index cid = mesh.get_facet_corner_begin(fid); cid < mesh.get_facet_corner_end(fid);
                 cid++) {
                Index vid = mesh.get_corner_vertex(cid);
                if (vid >= input_num_vertices) {
                    // cid is a barycenter corner.
                    flood_without_crossing_seams(cid);
                }
            }
        }

        value_attr.insert_elements({additional_values.data(), additional_values.size()});
    });
}

/**
 * Propagates attributes to the newly filled holes in the given mesh.
 *
 * @tparam Scalar The scalar type used for the mesh.
 * @tparam Index The index type used for the mesh.
 *
 * @param mesh The surface mesh with holes filled to which attributes will be propagated.
 * @param input_num_vertices The number of vertices in the original mesh with holes.
 * @param input_num_facets The number of facets in the original mesh with holes.
 */
template <typename Scalar, typename Index>
void propagate_attributes(
    SurfaceMesh<Scalar, Index>& mesh,
    Index input_num_vertices,
    Index input_num_facets,
    Index input_num_corners)
{
    Index num_vertices = mesh.get_num_vertices();
    Index num_facets = mesh.get_num_facets();

    process_vertex_attributes(mesh, input_num_vertices, num_vertices);
    process_facet_attributes(mesh, input_num_facets, num_facets);
    process_corner_attributes(
        mesh,
        input_num_vertices,
        input_num_facets,
        input_num_corners,
        num_facets);
    process_edge_attributes(mesh, input_num_facets);
    process_indexed_attributes(mesh, input_num_vertices, input_num_facets, num_facets);
}

} // namespace

template <typename Scalar, typename Index>
void close_small_holes(SurfaceMesh<Scalar, Index>& mesh, CloseSmallHolesOptions options)
{
    const auto dim = mesh.get_dimension();
    la_runtime_assert(dim == 2 || dim == 3, "Only 2D and 3D meshes are supported");

    mesh.initialize_edges();

    Index input_num_vertices = mesh.get_num_vertices();
    Index input_num_facets = mesh.get_num_facets();
    Index input_num_corners = mesh.get_num_corners();
    auto bd_loops = extract_boundary_loops(mesh);
    size_t num_loops = bd_loops.size();

    // Some holes cannot be filled by simply adding a polygon because an index attribute may have a
    // seam going through the hole. In this case, we need to add a center vertex to route the seam
    // through the hole.
    std::vector<bool> needs_barycenter(num_loops, false);
    for (size_t loop_id = 0; loop_id < num_loops; ++loop_id) {
        const auto& loop = bd_loops[loop_id];
        la_debug_assert(loop.size() >= 3, "Holes of size 1 or 2 shouldn't be possible");

        size_t loop_size = loop.size();
        if (loop_size <= options.max_hole_size) {
            bool add_barycenter = false;
            seq_foreach_attribute_read<AttributeElement::Indexed>(mesh, [&](auto&& attr) {
                if (add_barycenter) return;
                auto indices = matrix_view(attr.indices());
                for (size_t i = 0; i < loop_size; i++) {
                    Index vid = loop[i];
                    Index attr_index_on_bd = indices(mesh.get_one_corner_around_vertex(vid));
                    mesh.foreach_corner_around_vertex(vid, [&](Index cid) {
                        if (!add_barycenter && indices(cid) != attr_index_on_bd) {
                            add_barycenter = true;
                        }
                    });
                }
            });
            needs_barycenter[loop_id] = add_barycenter;
        }
    }

    // Modify the geometry of the mesh to close the holes.
    for (size_t loop_id = 0; loop_id < num_loops; ++loop_id) {
        auto& loop = bd_loops[loop_id];
        if (loop.size() <= options.max_hole_size) {
            // Reverse loop orientation so that the added polygon is ccw-oriented.
            std::reverse(loop.begin(), loop.end());

            if (!needs_barycenter[loop_id]) {
                mesh.add_polygon(loop);
            } else {
                auto vertices = vertex_view(mesh);
                Eigen::Matrix<Scalar, 1, Eigen::Dynamic, 1, 1, 3> barycenter(1, dim);
                barycenter.setZero();
                for (auto vid : loop) {
                    barycenter += vertices.row(vid);
                }
                barycenter /= static_cast<Scalar>(loop.size());
                Index barycenter_vid = mesh.get_num_vertices();
                mesh.add_vertex({barycenter.data(), static_cast<size_t>(dim)});
                mesh.add_triangles(static_cast<Index>(loop.size()), [&](Index i, span<Index> tri) {
                    Index vid = loop[i];
                    Index next_vid = loop[(i + 1) % loop.size()];
                    tri[0] = vid;
                    tri[1] = next_vid;
                    tri[2] = barycenter_vid;
                });
            }
        }
    }

    propagate_attributes(mesh, input_num_vertices, input_num_facets, input_num_corners);

    if (options.triangulate_holes) {
        triangulate_polygonal_facets(mesh);
    }
}

#define LA_X_close_small_holes(_, Scalar, Index)                \
    template LA_CORE_API void close_small_holes<Scalar, Index>( \
        SurfaceMesh<Scalar, Index>&,                            \
        CloseSmallHolesOptions);
LA_SURFACE_MESH_X(close_small_holes, 0)

} // namespace lagrange
