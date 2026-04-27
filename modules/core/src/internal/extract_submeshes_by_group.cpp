/*
 * Copyright 2026 Adobe. All rights reserved.
 * This file is licensed to you under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License. You may obtain a copy
 * of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under
 * the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
 * OF ANY KIND, either express or implied. See the License for the specific language
 * governing permissions and limitations under the License.
 */

#include "extract_submeshes_by_group.h"

#include <lagrange/Attribute.h>
#include <lagrange/Logger.h>
#include <lagrange/SurfaceMeshTypes.h>
#include <lagrange/foreach_attribute.h>
#include <lagrange/internal/map_attributes.h>
#include <lagrange/utils/invalid.h>
#include <lagrange/views.h>

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <tbb/enumerable_thread_specific.h>
#include <tbb/parallel_for.h>
#include <lagrange/utils/warnon.h>
// clang-format on

#include <algorithm>
#include <vector>

namespace lagrange::internal {

namespace {

// Thread-local scratch state reused across groups processed by the same thread.
template <typename Index>
struct ThreadLocalState
{
    // Vertex old-to-new remapping (size V, entries reset per group).
    std::vector<Index> vertex_old2new;

    // Per-group extraction buffers (cleared and reused each iteration).
    std::vector<Index> vertex_new2old;
    std::vector<Index> facet_new2old;
    std::vector<Index> corner_new2old;
    std::vector<Index> local_connectivity;
    std::vector<Index> facet_sizes; // hybrid meshes only

    // Scratch for indexed attribute compaction.
    std::vector<Index> value_old2new;
    std::vector<Index> value_new2old;
    std::vector<Index> output_indices;

    void clear_group_state()
    {
        vertex_new2old.clear();
        facet_new2old.clear();
        corner_new2old.clear();
        local_connectivity.clear();
        facet_sizes.clear();
    }
};

} // namespace

template <typename Scalar, typename Index>
std::vector<SurfaceMesh<Scalar, Index>> extract_submeshes_by_group(
    const SurfaceMesh<Scalar, Index>& mesh,
    size_t num_groups,
    span<const Index> facet_indices,
    span<const Index> group_offsets,
    const SubmeshOptions& options)
{
    const Index num_vertices = mesh.get_num_vertices();
    const bool is_regular = mesh.is_regular();
    const Index vertex_per_facet =
        is_regular && mesh.get_num_facets() > 0 ? mesh.get_facet_size(0) : 0;
    const Index dim = mesh.get_dimension();

    std::vector<SurfaceMesh<Scalar, Index>> results(num_groups);

    tbb::enumerable_thread_specific<ThreadLocalState<Index>> tls([&]() {
        ThreadLocalState<Index> s;
        s.vertex_old2new.assign(num_vertices, invalid<Index>());
        return s;
    });

    tbb::parallel_for(size_t{0}, num_groups, [&](size_t g) {
        auto& local = tls.local();
        local.clear_group_state();

        // -------------------------------------------------------------------
        // Phase 1: Extract group topology
        // -------------------------------------------------------------------

        auto& vertex_old2new = local.vertex_old2new;
        const size_t begin = static_cast<size_t>(group_offsets[g]);
        const size_t end = static_cast<size_t>(group_offsets[g + 1]);
        const size_t num_facets_g = end - begin;

        local.facet_new2old.reserve(num_facets_g);
        Index next_local_vertex = 0;

        for (size_t idx = begin; idx < end; ++idx) {
            const Index fid = facet_indices[idx];
            local.facet_new2old.push_back(fid);

            auto f = mesh.get_facet_vertices(fid);
            const Index fsize = static_cast<Index>(f.size());
            if (!is_regular) {
                local.facet_sizes.push_back(fsize);
            }

            const Index source_corner_begin = mesh.get_facet_corner_begin(fid);

            for (Index lv = 0; lv < fsize; ++lv) {
                const Index source_vid = f[lv];
                if (vertex_old2new[source_vid] == invalid<Index>()) {
                    vertex_old2new[source_vid] = next_local_vertex++;
                    local.vertex_new2old.push_back(source_vid);
                }
                local.local_connectivity.push_back(vertex_old2new[source_vid]);
                local.corner_new2old.push_back(source_corner_begin + lv);
            }
        }

        // Reset vertex_old2new (cost proportional to this group, not V).
        for (const Index source_vid : local.vertex_new2old) {
            vertex_old2new[source_vid] = invalid<Index>();
        }

        // -------------------------------------------------------------------
        // Phase 2: Build output mesh
        // -------------------------------------------------------------------

        const Index num_verts_g = static_cast<Index>(local.vertex_new2old.size());
        const Index num_facets_g_idx = static_cast<Index>(local.facet_new2old.size());

        SurfaceMesh<Scalar, Index> out(dim);

        // Add vertices and copy positions.
        out.add_vertices(num_verts_g);
        {
            auto in_vertices = vertex_view(mesh);
            auto out_vertices = vertex_ref(out);
            for (Index i = 0; i < num_verts_g; ++i) {
                out_vertices.row(i) = in_vertices.row(local.vertex_new2old[i]);
            }
        }

        // Add facets from pre-built local connectivity.
        if (num_facets_g_idx > 0) {
            if (is_regular) {
                out.add_polygons(num_facets_g_idx, vertex_per_facet);
                auto out_facets = facet_ref(out);
                std::copy(
                    local.local_connectivity.begin(),
                    local.local_connectivity.end(),
                    out_facets.data());
            } else {
                Index conn_offset = 0;
                out.add_hybrid(
                    num_facets_g_idx,
                    [&](Index fi) { return local.facet_sizes[fi]; },
                    [&](Index /*fi*/, span<Index> f) {
                        std::copy(
                            local.local_connectivity.data() + conn_offset,
                            local.local_connectivity.data() + conn_offset +
                                static_cast<Index>(f.size()),
                            f.begin());
                        conn_offset += static_cast<Index>(f.size());
                    });
                out.compress_if_regular();
            }
        }

        // Create source mapping attributes.
        if (!options.source_vertex_attr_name.empty()) {
            out.template create_attribute<Index>(
                options.source_vertex_attr_name,
                AttributeElement::Vertex,
                AttributeUsage::Scalar,
                1,
                {local.vertex_new2old.data(), static_cast<size_t>(num_verts_g)});
        }
        if (!options.source_facet_attr_name.empty()) {
            out.template create_attribute<Index>(
                options.source_facet_attr_name,
                AttributeElement::Facet,
                AttributeUsage::Scalar,
                1,
                {local.facet_new2old.data(), static_cast<size_t>(num_facets_g_idx)});
        }

        // Map attributes.
        if (options.map_attributes) {
            // Vertex attributes.
            map_attributes<Vertex>(
                mesh,
                out,
                {local.vertex_new2old.data(), local.vertex_new2old.size()});

            // Facet attributes.
            map_attributes<Facet>(
                mesh,
                out,
                {local.facet_new2old.data(), local.facet_new2old.size()});

            // Corner attributes.
            map_attributes<Corner>(
                mesh,
                out,
                {local.corner_new2old.data(), local.corner_new2old.size()});

            // Indexed attributes: build compact per-group value tables.
            // Scratch vectors live in TLS to amortize allocation across groups and attributes.
            seq_foreach_named_attribute_read<Indexed>(
                mesh,
                [&](std::string_view name, auto&& attr) {
                    using AttributeType = std::decay_t<decltype(attr)>;
                    using ValueType = typename AttributeType::ValueType;
                    if (out.attr_name_is_reserved(name)) return;

                    const auto& src_values = attr.values();
                    const auto& src_indices = attr.indices();
                    const Index num_channels = static_cast<Index>(attr.get_num_channels());
                    const Index num_corners_g = static_cast<Index>(local.corner_new2old.size());
                    const size_t num_src_values =
                        static_cast<size_t>(src_values.get_num_elements());

                    // Grow value_old2new lazily; reset only entries touched by the previous
                    // attribute (avoids O(total_indexed_values) clear per attribute per group).
                    if (local.value_old2new.size() < num_src_values) {
                        local.value_old2new.resize(num_src_values, invalid<Index>());
                    }
                    local.value_new2old.clear();
                    local.output_indices.resize(num_corners_g);

                    for (Index ci = 0; ci < num_corners_g; ++ci) {
                        const Index src_ci = local.corner_new2old[ci];
                        const Index src_val_id = src_indices.get(src_ci);
                        if (local.value_old2new[src_val_id] == invalid<Index>()) {
                            local.value_old2new[src_val_id] =
                                static_cast<Index>(local.value_new2old.size());
                            local.value_new2old.push_back(src_val_id);
                        }
                        local.output_indices[ci] = local.value_old2new[src_val_id];
                    }

                    // Create indexed attribute with compact values.
                    auto id = out.template create_attribute<ValueType>(
                        name,
                        Indexed,
                        attr.get_usage(),
                        num_channels);
                    auto& target_attr = out.template ref_indexed_attribute<ValueType>(id);

                    // Resize and fill compact values.
                    const Index num_compact_values = static_cast<Index>(local.value_new2old.size());
                    target_attr.values().resize_elements(num_compact_values);
                    for (Index i = 0; i < num_compact_values; ++i) {
                        const Index src_val_id = local.value_new2old[i];
                        for (Index ch = 0; ch < num_channels; ++ch) {
                            target_attr.values().ref(i, ch) = src_values.get(src_val_id, ch);
                        }
                    }

                    // Fill output indices.
                    auto& target_indices = target_attr.indices();
                    for (Index ci = 0; ci < num_corners_g; ++ci) {
                        target_indices.ref(ci) = local.output_indices[ci];
                    }

                    // Reset value_old2new for the next attribute
                    for (const Index old_id : local.value_new2old) {
                        local.value_old2new[old_id] = invalid<Index>();
                    }
                });

            // Edge attributes: warn and skip, matching extract_submesh behavior.
            {
                bool has_edge_attr = false;
                seq_foreach_attribute_read<AttributeElement::Edge>(mesh, [&](auto&&) {
                    has_edge_attr = true;
                });
                if (has_edge_attr) {
                    logger().warn(
                        "`separate_by_facet_groups`: Edge attributes remapping is not supported.");
                }
            }
        }

        results[g] = std::move(out);
    });

    return results;
}

#define LA_X_extract_submeshes_by_group(_, Scalar, Index)                                    \
    template LA_CORE_API std::vector<SurfaceMesh<Scalar, Index>> extract_submeshes_by_group( \
        const SurfaceMesh<Scalar, Index>&,                                                   \
        size_t,                                                                              \
        span<const Index>,                                                                   \
        span<const Index>,                                                                   \
        const SubmeshOptions&);

LA_SURFACE_MESH_X(extract_submeshes_by_group, 0)

} // namespace lagrange::internal
