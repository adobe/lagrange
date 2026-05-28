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
#include <lagrange/compute_uv_orientation.h>
#include <lagrange/get_unique_attribute_name.h>
#include <lagrange/internal/compact_chart_ids.h>
#include <lagrange/internal/get_uv_attribute.h>
#include <lagrange/unflip_uv_charts.h>
#include <lagrange/utils/Error.h>
#include <lagrange/utils/assert.h>
#include <lagrange/utils/scope_guard.h>
#include <lagrange/utils/triangle_area.h>
#include <lagrange/uv_mesh.h>
#include <lagrange/views.h>

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <tbb/enumerable_thread_specific.h>
#include <tbb/parallel_for.h>
#include <lagrange/utils/warnon.h>
// clang-format on

#include <atomic>
#include <vector>

namespace lagrange {

namespace {

template <typename UVScalar, typename Index>
struct ChartStatsT
{
    UVScalar signed_area = 0;
    Index num_facets = 0;
    Index num_flipped = 0;
    Index num_degenerate = 0;
};

template <typename UVScalar, typename Scalar, typename Index>
size_t unflip_uv_charts_impl(
    SurfaceMesh<Scalar, Index>& mesh,
    AttributeId uv_attr_id,
    span<const Index> chart_ids,
    span<const int8_t> uv_orientation)
{
    auto& uv_attr = mesh.template ref_indexed_attribute<UVScalar>(uv_attr_id);
    auto uv_values = matrix_ref(uv_attr.values());
    auto uv_indices = vector_view(uv_attr.indices());

    const Index num_facets = mesh.get_num_facets();
    if (num_facets == 0) return 0;

    auto compacted = internal::compact_chart_ids<Index>(chart_ids);
    const auto& dense_chart_ids = compacted.first;
    const Index num_charts = compacted.second;

    // Per-thread accumulator: bounded by hardware concurrency (not by tbb task splits).
    using ChartStats = ChartStatsT<UVScalar, Index>;
    tbb::enumerable_thread_specific<std::vector<ChartStats>> tls_stats(
        [&]() { return std::vector<ChartStats>(num_charts); });

    tbb::parallel_for(
        tbb::blocked_range<Index>(0, num_facets),
        [&](const tbb::blocked_range<Index>& r) {
            auto& local = tls_stats.local();
            for (Index f = r.begin(); f != r.end(); ++f) {
                const auto c = mesh.get_facet_corner_begin(f);
                const Index i0 = uv_indices[c + 0];
                const Index i1 = uv_indices[c + 1];
                const Index i2 = uv_indices[c + 2];
                const std::array<UVScalar, 2> p0 = {uv_values(i0, 0), uv_values(i0, 1)};
                const std::array<UVScalar, 2> p1 = {uv_values(i1, 0), uv_values(i1, 1)};
                const std::array<UVScalar, 2> p2 = {uv_values(i2, 0), uv_values(i2, 1)};
                const UVScalar abs_area = std::abs(
                    triangle_area_2d<UVScalar>(
                        span<const UVScalar, 2>(p0.data(), 2),
                        span<const UVScalar, 2>(p1.data(), 2),
                        span<const UVScalar, 2>(p2.data(), 2)));
                const int8_t orient = uv_orientation[f];
                const UVScalar sign = orient > 0   ? UVScalar(1)
                                      : orient < 0 ? UVScalar(-1)
                                                   : UVScalar(0);
                auto& s = local[dense_chart_ids[f]];
                s.signed_area += sign * abs_area;
                s.num_facets += 1;
                s.num_flipped += (orient < 0) ? 1 : 0;
                s.num_degenerate += (orient == 0) ? 1 : 0;
            }
        });

    std::vector<ChartStats> chart_stats(num_charts);
    for (const auto& v : tls_stats) {
        for (Index ci = 0; ci < num_charts; ++ci) {
            chart_stats[ci].signed_area += v[ci].signed_area;
            chart_stats[ci].num_facets += v[ci].num_facets;
            chart_stats[ci].num_flipped += v[ci].num_flipped;
            chart_stats[ci].num_degenerate += v[ci].num_degenerate;
        }
    }

    std::vector<uint8_t> chart_flipped(num_charts, 0);
    size_t num_flipped = 0;
    for (Index ci = 0; ci < num_charts; ++ci) {
        const auto& s = chart_stats[ci];
        // No positively-oriented triangles, but at least one flipped one (degenerates allowed).
        const bool fully_flipped =
            s.num_flipped > 0 && s.num_flipped + s.num_degenerate == s.num_facets;
        if (s.signed_area < UVScalar(0) || fully_flipped) {
            chart_flipped[ci] = 1;
            ++num_flipped;
        }
    }
    if (num_flipped == 0) return 0;

    // Mark every UV vertex referenced by a facet in a flipped chart, then negate its U coordinate.
    // Assumes UV vertices are not shared across charts (the typical case for charts produced by
    // disconnect_uv_charts or compute_uv_charts on indexed UV attributes).
    // Atomic flags: multiple facets can share a UV vertex, so concurrent writes of the same
    // value are benign but flagged by ThreadSanitizer without atomic accesses.
    const Index num_uv_vertices = static_cast<Index>(uv_values.rows());
    std::vector<std::atomic<uint8_t>> uv_vertex_flipped(num_uv_vertices);
    tbb::parallel_for(Index(0), num_uv_vertices, [&](Index v) {
        uv_vertex_flipped[v].store(0, std::memory_order_relaxed);
    });
    tbb::parallel_for(Index(0), num_facets, [&](Index f) {
        if (!chart_flipped[dense_chart_ids[f]]) return;
        const auto c_begin = mesh.get_facet_corner_begin(f);
        const auto c_end = mesh.get_facet_corner_end(f);
        for (auto c = c_begin; c != c_end; ++c) {
            uv_vertex_flipped[uv_indices[c]].store(1, std::memory_order_relaxed);
        }
    });
    tbb::parallel_for(Index(0), num_uv_vertices, [&](Index v) {
        if (uv_vertex_flipped[v].load(std::memory_order_relaxed)) {
            uv_values(v, 0) = -uv_values(v, 0);
        }
    });

    logger().info("Unflipped {} UV chart(s).", num_flipped);
    return num_flipped;
}

} // namespace

template <typename Scalar, typename Index>
size_t unflip_uv_charts(SurfaceMesh<Scalar, Index>& mesh, const UnflipUVChartsOptions& options)
{
    la_runtime_assert(mesh.is_triangle_mesh(), "unflip_uv_charts: mesh must be a triangle mesh.");

    UVMeshOptions uv_mesh_options;
    uv_mesh_options.uv_attribute_name = options.uv_attribute_name;

    return internal::dispatch_uv_scalar_type(
        mesh,
        uv_mesh_options,
        "unflip_uv_charts",
        [&](auto tag, AttributeId uv_attr_id) -> size_t {
            using UVScalar = typename decltype(tag)::type;
            if (!mesh.is_attribute_indexed(uv_attr_id)) {
                throw Error("unflip_uv_charts: UV attribute must be indexed.");
            }
            std::string uv_attr_name(mesh.get_attribute_name(uv_attr_id));

            std::string chart_attr_name;
            auto cleanup_guard = make_scope_guard([&]() noexcept {
                if (!chart_attr_name.empty() && mesh.has_attribute(chart_attr_name)) {
                    mesh.delete_attribute(chart_attr_name);
                }
            });

            if (options.chart_id_attribute_name.empty()) {
                chart_attr_name = get_unique_attribute_name(mesh, "@_unflip_uv_charts_tmp");
                UVChartOptions chart_options;
                chart_options.uv_attribute_name = uv_attr_name;
                chart_options.output_attribute_name = chart_attr_name;
                compute_uv_charts(mesh, chart_options);
            } else {
                cleanup_guard.dismiss();
                chart_attr_name = options.chart_id_attribute_name;
            }

            if (!mesh.has_attribute(chart_attr_name)) {
                throw Error("unflip_uv_charts: chart ID attribute does not exist.");
            }
            auto chart_id_attr_id = mesh.get_attribute_id(chart_attr_name);
            if (mesh.get_attribute_base(chart_id_attr_id).get_element_type() !=
                AttributeElement::Facet) {
                throw Error("unflip_uv_charts: chart ID attribute must be a facet attribute.");
            }
            auto chart_ids = attribute_vector_view<Index>(mesh, chart_id_attr_id);
            if (static_cast<size_t>(chart_ids.size()) != mesh.get_num_facets()) {
                throw Error("unflip_uv_charts: chart ID attribute must have one value per facet.");
            }
            span<const Index> chart_ids_span{
                chart_ids.data(),
                static_cast<size_t>(chart_ids.size())};

            std::string orient_attr_name =
                get_unique_attribute_name(mesh, "@_unflip_uv_orient_tmp");
            auto orient_cleanup = make_scope_guard([&]() noexcept {
                if (mesh.has_attribute(orient_attr_name)) mesh.delete_attribute(orient_attr_name);
            });
            UVOrientationOptions orient_options;
            orient_options.uv_attribute_name = uv_attr_name;
            orient_options.output_attribute_name = orient_attr_name;
            compute_uv_orientation(mesh, orient_options);
            auto uv_orientation =
                attribute_vector_view<int8_t>(mesh, mesh.get_attribute_id(orient_attr_name));
            span<const int8_t> uv_orientation_span{
                uv_orientation.data(),
                static_cast<size_t>(uv_orientation.size())};

            return unflip_uv_charts_impl<UVScalar>(
                mesh,
                uv_attr_id,
                chart_ids_span,
                uv_orientation_span);
        });
}

#define LA_X_unflip_uv_charts(_, Scalar, Index)                  \
    template LA_CORE_API size_t unflip_uv_charts<Scalar, Index>( \
        SurfaceMesh<Scalar, Index>&,                             \
        const UnflipUVChartsOptions&);
LA_SURFACE_MESH_X(unflip_uv_charts, 0)

} // namespace lagrange
