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
#include <lagrange/Attribute.h>
#include <lagrange/IndexedAttribute.h>
#include <lagrange/Logger.h>
#include <lagrange/SurfaceMeshTypes.h>
#include <lagrange/compute_uv_charts.h>
#include <lagrange/disconnect_uv_charts.h>
#include <lagrange/find_matching_attributes.h>
#include <lagrange/get_unique_attribute_name.h>
#include <lagrange/internal/get_uv_attribute.h>
#include <lagrange/internal/invert_mapping.h>
#include <lagrange/utils/DisjointSets.h>
#include <lagrange/utils/Error.h>
#include <lagrange/utils/assert.h>
#include <lagrange/utils/invalid.h>
#include <lagrange/utils/scope_guard.h>
#include <lagrange/uv_mesh.h>
#include <lagrange/views.h>

#include <array>
#include <string>
#include <utility>
#include <vector>

namespace lagrange {

namespace {

template <typename UVScalar, typename Scalar, typename Index>
size_t disconnect_uv_charts_impl(
    SurfaceMesh<Scalar, Index>& mesh,
    AttributeId uv_attr_id,
    span<const Index> chart_ids)
{
    auto& uv_attr = mesh.template ref_indexed_attribute<UVScalar>(uv_attr_id);
    auto old_values = matrix_view(uv_attr.values());
    auto uv_indices = vector_ref(uv_attr.indices());

    const Index num_old_values = static_cast<Index>(old_values.rows());
    const Index num_cols = static_cast<Index>(old_values.cols());

    const Index num_facets = mesh.get_num_facets();

    // Pass 1: chart split. Dedupe (uv_idx, chart_id) -> new_uv_idx via a counting-sort layout
    // (Liani, https://maxliani.wordpress.com/2025/03/01/mesh-edge-construction/): count corners
    // per uv_idx, prefix-sum to offsets, then for each corner linearly scan its small bucket
    // (avg ~1-2 entries — one per chart touching that uv) for a matching chart_id. Counting
    // chart duplicates explicitly (a bucket gaining its 2nd+ entry) stays correct when the input
    // has unreferenced UV values that get compacted away here.
    std::vector<Index> bucket_count(static_cast<size_t>(num_old_values), Index(0));
    for (Index f = 0; f < num_facets; ++f) {
        const auto c_begin = mesh.get_facet_corner_begin(f);
        const auto c_end = mesh.get_facet_corner_end(f);
        for (auto c = c_begin; c < c_end; ++c) {
            ++bucket_count[static_cast<size_t>(uv_indices[c])];
        }
    }
    std::vector<Index> bucket_offsets(static_cast<size_t>(num_old_values) + 1, Index(0));
    for (Index v = 0; v < num_old_values; ++v) {
        bucket_offsets[v + 1] = bucket_offsets[v] + bucket_count[v];
    }
    std::fill(bucket_count.begin(), bucket_count.end(), Index(0));
    std::vector<Index> bucket_chart(static_cast<size_t>(bucket_offsets.back()));
    std::vector<Index> bucket_new_idx(static_cast<size_t>(bucket_offsets.back()));

    std::vector<std::array<UVScalar, 2>> new_values;
    new_values.reserve(static_cast<size_t>(num_old_values));
    size_t num_chart_duplicated = 0;

    for (Index f = 0; f < num_facets; ++f) {
        const Index chart = chart_ids[f];
        const auto c_begin = mesh.get_facet_corner_begin(f);
        const auto c_end = mesh.get_facet_corner_end(f);
        for (auto c = c_begin; c < c_end; ++c) {
            const Index uv_idx = uv_indices[c];
            const Index base = bucket_offsets[uv_idx];
            const Index count = bucket_count[uv_idx];
            Index new_idx = invalid<Index>();
            for (Index k = 0; k < count; ++k) {
                if (bucket_chart[base + k] == chart) {
                    new_idx = bucket_new_idx[base + k];
                    break;
                }
            }
            if (new_idx == invalid<Index>()) {
                new_idx = static_cast<Index>(new_values.size());
                bucket_chart[base + count] = chart;
                bucket_new_idx[base + count] = new_idx;
                bucket_count[uv_idx] = count + 1;
                if (count > 0) ++num_chart_duplicated;
                std::array<UVScalar, 2> val;
                for (Index col = 0; col < num_cols; ++col) {
                    val[col] = old_values(uv_idx, col);
                }
                new_values.push_back(val);
            }
            uv_indices[c] = new_idx;
        }
    }

    // Pass 2: split bowtie / pinch-point UV vertices. After the chart-based remap above, two
    // facets that belong to the same chart but only share a single UV vertex (no UV edge) still
    // reference that vertex through a single index. This makes the UV mesh non-manifold at that
    // vertex and breaks downstream consumers (e.g. repack_uv_charts) that assume charts are
    // vertex-disjoint. Here we partition the corners incident at each UV vertex into UV-edge-
    // connected wedges and duplicate the vertex per extra wedge.
    const size_t num_after_chart_split = new_values.size();

    // Flat CSR of corners-per-uv: target-to-source inverse of corner -> uv_indices[corner].
    const Index num_corners = static_cast<Index>(uv_indices.size());
    auto corners_per_uv = internal::invert_mapping<Index>(
        num_corners,
        [&](Index c) -> Index { return uv_indices[c]; },
        static_cast<Index>(num_after_chart_split));
    const auto& corner_offsets = corners_per_uv.offsets;
    const auto& corner_indices = corners_per_uv.data;

    // Per-vertex scratch, reused across iterations. `neighbor_to_local` holds (neighbor_uv,
    // local_corner_idx) pairs — valence is small (typ. ≤ 6, so ≤ 12 probes), linear scan suffices.
    // `root_to_target` is indexed by DisjointSets root, which is itself a local corner index in
    // [0, num_inc). `dirty_roots` tracks slots touched this iteration so we reset only those.
    DisjointSets<Index> uf;
    std::vector<std::pair<Index, Index>> neighbor_to_local;
    std::vector<Index> root_to_target;
    std::vector<Index> dirty_roots;
    size_t num_bowtie_duplicated = 0;
    for (Index v = 0; v < static_cast<Index>(num_after_chart_split); ++v) {
        const Index c_off = corner_offsets[v];
        const Index num_inc = corner_offsets[v + 1] - c_off;
        if (num_inc <= 1) continue;
        const Index* corners = corner_indices.data() + c_off;

        uf.init(static_cast<size_t>(num_inc));
        neighbor_to_local.clear();

        // Two incident corners belong to the same wedge if they share a UV edge through `v`, i.e.
        // their facet's prev/next corner points to the same neighbor UV index.
        for (Index i = 0; i < num_inc; ++i) {
            const Index c = corners[i];
            const Index f = mesh.get_corner_facet(c);
            const auto fc_begin = mesh.get_facet_corner_begin(f);
            const auto fc_end = mesh.get_facet_corner_end(f);
            const Index fc_size = static_cast<Index>(fc_end - fc_begin);
            const Index k = c - fc_begin;
            const Index prev_c = fc_begin + (k + fc_size - 1) % fc_size;
            const Index next_c = fc_begin + (k + 1) % fc_size;
            for (Index n : {uv_indices[prev_c], uv_indices[next_c]}) {
                bool merged = false;
                for (const auto& [nb, li] : neighbor_to_local) {
                    if (nb == n) {
                        uf.merge(i, li);
                        merged = true;
                        break;
                    }
                }
                if (!merged) neighbor_to_local.emplace_back(n, i);
            }
        }

        // First wedge keeps index v; each additional wedge gets a duplicate of v.
        if (static_cast<Index>(root_to_target.size()) < num_inc) {
            root_to_target.resize(static_cast<size_t>(num_inc), invalid<Index>());
        }
        dirty_roots.clear();
        for (Index i = 0; i < num_inc; ++i) {
            const Index r = uf.find(i);
            if (root_to_target[r] != invalid<Index>()) continue;
            if (dirty_roots.empty()) {
                root_to_target[r] = v;
            } else {
                root_to_target[r] = static_cast<Index>(new_values.size());
                new_values.push_back(new_values[v]);
                ++num_bowtie_duplicated;
            }
            dirty_roots.push_back(r);
        }
        for (Index i = 0; i < num_inc; ++i) {
            const Index target = root_to_target[uf.find(i)];
            if (target != v) uv_indices[corners[i]] = target;
        }
        for (Index r : dirty_roots) root_to_target[r] = invalid<Index>();
    }

    const size_t num_duplicated = num_chart_duplicated + num_bowtie_duplicated;

    // Rewrite UV values
    uv_attr.values().resize_elements(new_values.size());
    auto new_val_ref = matrix_ref(uv_attr.values());
    for (size_t i = 0; i < new_values.size(); ++i) {
        for (Index c = 0; c < num_cols; ++c) {
            new_val_ref(i, c) = new_values[i][c];
        }
    }

    if (num_duplicated > 0) {
        logger().info(
            "Disconnected UV charts: duplicated {} UV vertices ({} chart, {} bowtie) ({} -> {}).",
            num_duplicated,
            num_chart_duplicated,
            num_bowtie_duplicated,
            num_old_values,
            new_values.size());
    }

    return num_duplicated;
}

} // namespace

template <typename Scalar, typename Index>
size_t disconnect_uv_charts(
    SurfaceMesh<Scalar, Index>& mesh,
    const DisconnectUVChartsOptions& options)
{
    // Resolve UV attribute name (indexed UV attributes only)
    UVMeshOptions uv_mesh_options;
    uv_mesh_options.uv_attribute_name = options.uv_attribute_name;
    uv_mesh_options.element_types = UVMeshOptions::ElementTypes::IndexedOrVertex;

    return internal::dispatch_uv_scalar_type(
        mesh,
        uv_mesh_options,
        "disconnect_uv_charts",
        [&](auto tag, AttributeId uv_attr_id) -> size_t {
            using UVScalar = typename decltype(tag)::type;
            if (!mesh.is_attribute_indexed(uv_attr_id)) {
                throw Error(
                    "disconnect_uv_charts: UV attribute must be indexed. "
                    "Found a vertex UV attribute, but this function requires an indexed UV "
                    "attribute.");
            }
            std::string uv_attr_name(mesh.get_attribute_name(uv_attr_id));

            // Get or compute chart ids
            std::string chart_attr_name;
            auto cleanup_guard = make_scope_guard([&]() noexcept {
                if (!chart_attr_name.empty() && mesh.has_attribute(chart_attr_name)) {
                    mesh.delete_attribute(chart_attr_name);
                }
            });

            if (options.chart_id_attribute_name.empty()) {
                chart_attr_name = get_unique_attribute_name(mesh, "@_disconnect_uv_charts_tmp");
                UVChartOptions chart_options;
                chart_options.uv_attribute_name = uv_attr_name;
                chart_options.output_attribute_name = chart_attr_name;
                compute_uv_charts(mesh, chart_options);
            } else {
                cleanup_guard.dismiss();
                chart_attr_name = options.chart_id_attribute_name;
            }

            if (!mesh.has_attribute(chart_attr_name)) {
                throw Error("disconnect_uv_charts: chart ID attribute does not exist.");
            }
            auto chart_id_attr_id = mesh.get_attribute_id(chart_attr_name);
            if (mesh.get_attribute_base(chart_id_attr_id).get_element_type() !=
                AttributeElement::Facet) {
                throw Error("disconnect_uv_charts: chart ID attribute must be a facet attribute.");
            }
            auto chart_ids = attribute_vector_view<Index>(mesh, chart_id_attr_id);
            if (static_cast<size_t>(chart_ids.size()) != mesh.get_num_facets()) {
                throw Error(
                    "disconnect_uv_charts: chart ID attribute must have one value per facet.");
            }
            span<const Index> chart_ids_span{
                chart_ids.data(),
                static_cast<size_t>(chart_ids.size())};

            return disconnect_uv_charts_impl<UVScalar>(mesh, uv_attr_id, chart_ids_span);
        });
}

#define LA_X_disconnect_uv_charts(_, Scalar, Index)                  \
    template LA_CORE_API size_t disconnect_uv_charts<Scalar, Index>( \
        SurfaceMesh<Scalar, Index>&,                                 \
        const DisconnectUVChartsOptions&);
LA_SURFACE_MESH_X(disconnect_uv_charts, 0)

} // namespace lagrange
