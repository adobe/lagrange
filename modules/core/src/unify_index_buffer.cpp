/*
 * Copyright 2022 Adobe. All rights reserved.
 * This file is licensed to you under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License. You may obtain a copy
 * of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under
 * the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
 * OF ANY KIND, either express or implied. See the License for the specific language
 * governing permissions and limitations under the License.
 */
#include <lagrange/Logger.h>
#include <lagrange/SurfaceMeshTypes.h>
#include <lagrange/foreach_attribute.h>
#include <lagrange/unify_index_buffer.h>
#include <lagrange/utils/assert.h>
#include <lagrange/utils/invalid.h>
#include <lagrange/utils/range.h>
#include <lagrange/utils/warning.h>

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <tbb/parallel_for.h>
#include <lagrange/utils/warnon.h>
// clang-format on

#include <algorithm>
#include <iterator>
#include <numeric>

namespace lagrange {

template <typename Scalar, typename Index>
SurfaceMesh<Scalar, Index> unify_index_buffer(
    const SurfaceMesh<Scalar, Index>& mesh,
    const std::vector<AttributeId>& attribute_ids)
{
    enum CompResult : short { LESS = -1, EQUAL = 0, GREATER = 1 };
    std::vector<span<const Index>> selected_indices;
    selected_indices.reserve(attribute_ids.size());

    // Gather involved indices.
    seq_foreach_named_attribute_read<Indexed>(mesh, [&](std::string_view name, auto&& attr) {
        if (mesh.attr_name_is_reserved(name)) return;
        if (!attribute_ids.empty()) {
            if (std::find(
                    attribute_ids.begin(),
                    attribute_ids.end(),
                    mesh.get_attribute_id(name)) == attribute_ids.end())
                return;
        }
        selected_indices.push_back(attr.indices().get_all());
    });

    // Compare if corner c0 and c1 is the same in all indexed attributes.
    auto compare_corner = [&](Index c0, Index c1) -> CompResult {
        for (const auto& indices : selected_indices) {
            if (indices[c0] < indices[c1]) return LESS;
            if (indices[c0] > indices[c1]) return GREATER;
        }
        return EQUAL;
    };

    // Part 1: gather and group unique corners.
    const auto num_vertices = mesh.get_num_vertices();
    const auto num_corners = mesh.get_num_corners();
    std::vector<Index> corner_groups(num_corners);
    std::vector<size_t> corner_group_indices = {0};
    corner_group_indices.reserve(num_vertices * 2);

    {
        std::vector<size_t> vertex_corners(num_vertices + 1, 0);
        std::vector<size_t> vertex_corner_local_indices(num_vertices, 0);
        for (auto cid : range(num_corners)) {
            auto vid = mesh.get_corner_vertex(cid);
            vertex_corners[vid + 1]++;
        }
        std::partial_sum(vertex_corners.begin(), vertex_corners.end(), vertex_corners.begin());

        for (auto cid : range(num_corners)) {
            auto vid = mesh.get_corner_vertex(cid);
            auto i = vertex_corners[vid] + vertex_corner_local_indices[vid];
            corner_groups[i] = cid;
            vertex_corner_local_indices[vid]++;
            la_debug_assert(vertex_corner_local_indices[vid] <= vertex_corners[vid + 1]);
        }

        tbb::parallel_for(Index(0), num_vertices, [&](Index vid) {
            const auto num_vertex_corners = vertex_corners[vid + 1] - vertex_corners[vid];
            if (num_vertex_corners == 0) return;

            auto start_itr = std::next(corner_groups.begin(), vertex_corners[vid]);
            auto end_itr = std::next(corner_groups.begin(), vertex_corners[vid + 1]);
            std::sort(start_itr, end_itr, [&](Index c0, Index c1) {
                return compare_corner(c0, c1) == LESS;
            });
        });

        for (auto vid : range(num_vertices)) {
            auto start_itr = std::next(corner_groups.begin(), vertex_corners[vid]);
            auto end_itr = std::next(corner_groups.begin(), vertex_corners[vid + 1]);
            for (auto itr = start_itr; itr != end_itr; itr++) {
                auto next_itr = std::next(itr);
                if (next_itr == end_itr || compare_corner(*itr, *next_itr) != EQUAL) {
                    corner_group_indices.push_back(std::distance(corner_groups.begin(), next_itr));
                }
            }
        }
    }

    // Part 2: generate output mesh.
    SurfaceMesh<Scalar, Index> output_mesh;

    // Map vertices.
    std::vector<Index> corner_to_vertex(mesh.get_num_corners(), invalid<Index>());
    size_t num_unique_corners = corner_group_indices.size() - 1;
    logger().debug("Unified index buffer: {} vertices", num_unique_corners);
    output_mesh.add_vertices(static_cast<Index>(num_unique_corners), [&](Index i, span<Scalar> p) {
        for (size_t j = corner_group_indices[i]; j < corner_group_indices[i + 1]; j++) {
            const auto cid = corner_groups[j];
            corner_to_vertex[cid] = i;

            if (j == corner_group_indices[i]) {
                Index vid = mesh.get_corner_vertex(cid);
                const auto q = mesh.get_position(vid);
                std::copy(q.begin(), q.end(), p.begin());
            }
        }
    });

    // Map facets.
    auto num_facets = mesh.get_num_facets();
    logger().debug("Unified index buffer: {} facets", num_facets);
    output_mesh.add_hybrid(
        num_facets,
        [&](Index fid) { return mesh.get_facet_size(fid); },
        [&](Index fid, span<Index> t) {
            Index cid_begin = mesh.get_facet_corner_begin(fid);
            for (auto i : range(mesh.get_facet_size(fid))) {
                t[i] = corner_to_vertex[cid_begin + i];
            }
        });

    // Map vertex attributes.
    seq_foreach_named_attribute_read<Vertex>(mesh, [&](std::string_view name, auto&& attr) {
        if (mesh.attr_name_is_reserved(name)) return;

        logger().debug("Unified index buffer: mapping vertex attribute {}", name);
        using AttributeType = std::decay_t<decltype(attr)>;
        using ValueType = typename AttributeType::ValueType;
        auto id = output_mesh.template create_attribute<ValueType>(
            name,
            attr.get_element_type(),
            attr.get_usage(),
            attr.get_num_channels());

        auto& out_attr = output_mesh.template ref_attribute<ValueType>(id);
        out_attr.resize_elements(num_unique_corners);
        for (auto vid : range(num_unique_corners)) {
            auto cid = corner_groups[corner_group_indices[vid]];
            auto prev_vid = mesh.get_corner_vertex(cid);
            auto source_value = attr.get_row(prev_vid);
            auto target_value = out_attr.ref_row(vid);
            std::copy(source_value.begin(), source_value.end(), target_value.begin());
        }
    });

    // Map facet and corner attributes.
    seq_foreach_named_attribute_read<Facet | Corner>(mesh, [&](std::string_view name, auto&& attr) {
        LA_IGNORE(attr);
        if (mesh.attr_name_is_reserved(name)) return;
        logger().debug("Unified index buffer: mapping facet attribute {}", name);
        output_mesh.create_attribute_from(name, mesh);
    });

    // Map index attributes.
    seq_foreach_named_attribute_read<Indexed>(mesh, [&](std::string_view name, auto&& attr) {
        if (mesh.attr_name_is_reserved(name)) return;
        auto attr_id = mesh.get_attribute_id(name);

        if (!attribute_ids.empty() &&
            std::find(attribute_ids.begin(), attribute_ids.end(), attr_id) != attribute_ids.end()) {
            logger().debug(
                "Unified index buffer: mapping indexed attribute \"{}\" as vertex attribute",
                name);
            // Selected attributes should be mapped over as vertex
            // attribute.
            using AttributeType = std::decay_t<decltype(attr)>;
            using ValueType = typename AttributeType::ValueType;

            const auto& attr_values = attr.values();
            const auto& attr_indices = attr.indices();

            output_mesh.template create_attribute<ValueType>(
                name,
                Vertex,
                attr.get_usage(),
                attr.get_num_channels());

            auto& out_attr = output_mesh.template ref_attribute<ValueType>(name);
            out_attr.resize_elements(num_unique_corners);

            for (auto i : range(num_unique_corners)) {
                auto cid = corner_groups[corner_group_indices[i]];
                auto source_value = attr_values.get_row(attr_indices.get(cid));
                auto target_value = out_attr.ref_row(i);
                std::copy(source_value.begin(), source_value.end(), target_value.begin());
            }
        } else {
            // Copy over unselected index attribute.
            logger().debug(
                "Unified index buffer: mapping indexed attribute \"{}\" as indexed attribute",
                name);
            output_mesh.create_attribute_from(name, mesh);
        }
    });

    // Skip edge attribute for now.  Maybe TODO later.
    {
        bool has_edge_attr = false;
        seq_foreach_attribute_read<Edge>(mesh, [&](auto&&) { has_edge_attr = true; });
        if (has_edge_attr) {
            logger().warn(
                "Mesh has edge attributes. Those will not be remapped to the new triangular "
                "facets. Please remap them manually.");
        }
    }

    return output_mesh;
}

template <typename Scalar, typename Index>
SurfaceMesh<Scalar, Index> unify_named_index_buffer(
    const SurfaceMesh<Scalar, Index>& mesh,
    const std::vector<std::string_view>& attribute_names)
{
    std::vector<AttributeId> ids;
    ids.reserve(attribute_names.size());
    for (auto name : attribute_names) {
        ids.push_back(mesh.get_attribute_id(name));
    }
    return unify_index_buffer(mesh, ids);
}


#define LA_X_unify_index_buffer(_, Scalar, Index)                 \
    template SurfaceMesh<Scalar, Index> unify_index_buffer(       \
        const SurfaceMesh<Scalar, Index>& mesh,                   \
        const std::vector<AttributeId>& attribute_ids);           \
    template SurfaceMesh<Scalar, Index> unify_named_index_buffer( \
        const SurfaceMesh<Scalar, Index>& mesh,                   \
        const std::vector<std::string_view>& attribute_names);
LA_SURFACE_MESH_X(unify_index_buffer, 0)

} // namespace lagrange
