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

#include <lagrange/raycasting/remove_occluded_facets.h>

#include "OccludedFacetSampler.h"
#include "compute_graphcut_mask.h"
#include "occluded_sampler_common.h"
#include "solve_graphcut_with_view_error.h"

#include <lagrange/AttributeFwd.h>
#include <lagrange/Logger.h>
#include <lagrange/SurfaceMeshTypes.h>
#include <lagrange/compute_facet_facet_adjacency.h>
#include <lagrange/scene/filter_instances.h>
#include <lagrange/utils/assert.h>
#include <lagrange/utils/fmt/format.h>
#include <lagrange/utils/range.h>

#include <algorithm>
#include <cmath>
#include <memory>
#include <mutex>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

namespace lagrange::raycasting {

using namespace detail;

using FacetAdjacencyEdges = std::vector<std::pair<uint64_t, uint64_t>>;

template <typename Index>
struct FacetInstanceMeasures
{
    Index mesh_index;
    Index instance_index;
    std::vector<double> measures;
};

namespace {

struct AdjacencyCache
{
    std::once_flag once;
    std::vector<FacetAdjacencyEdges> adjacency_edges;
};

} // namespace

template <typename Scalar, typename Index>
struct OccludedFacetsFilter<Scalar, Index>::Impl
{
    const scene::SimpleScene<Scalar, Index, 3>& source;
    double max_visibility_threshold;
    std::vector<FacetInstanceMeasures<Index>> instances;
    std::unique_ptr<AdjacencyCache> adjacency_cache = std::make_unique<AdjacencyCache>();

    Impl(
        const scene::SimpleScene<Scalar, Index, 3>& scene,
        const OccludedFacetSamplerOptions& sampler_options,
        ProgressCallback& progress,
        function_ref<bool(Index, Index)> is_occluder,
        const std::atomic_bool* cancel);
};

template <typename Scalar, typename Index>
OccludedFacetsFilter<Scalar, Index>::~OccludedFacetsFilter() = default;

template <typename Scalar, typename Index>
OccludedFacetsFilter<Scalar, Index>::OccludedFacetsFilter(OccludedFacetsFilter&&) noexcept =
    default;

template <typename Scalar, typename Index>
OccludedFacetsFilter<Scalar, Index>& OccludedFacetsFilter<Scalar, Index>::operator=(
    OccludedFacetsFilter&&) noexcept = default;

template <typename Scalar, typename Index>
double OccludedFacetsFilter<Scalar, Index>::get_max_visibility_threshold() const
{
    la_runtime_assert(static_cast<bool>(m_impl), "OccludedFacetsFilter has been moved from");
    return m_impl->max_visibility_threshold;
}

namespace {

template <typename Scalar, typename Index>
void run_sampler(
    OccludedFacetSampler<Scalar, Index>& sampler,
    const OccludedFacetSamplerOptions& options,
    ProgressCallback& progress,
    const std::atomic_bool* cancel)
{
    la_runtime_assert(options.batch_size > 0);
    la_runtime_assert(
        options.num_rays > 0 || cancel != nullptr,
        "Facet estimation needs at least one termination condition: num_rays>0 or a "
        "non-null cancel flag");

    const uint64_t total_facets = sampler.num_facets();
    lagrange::logger().info("Searching for occluded facets");
    progress.set_section("Searching for occluded facets");

    uint64_t prev_total_rays = 0;
    while (true) {
        sampler.run_batch(options.batch_size);

        // This is the actual count: retired facets receive no rays, and weighted allocation may
        // differ slightly from the requested batch size.
        const auto [num_visible, total_rays] = sum_progress(sampler, total_facets);
        const uint64_t num_retired = sampler.num_retired();
        lagrange::logger().info(
            "{}/{} facets visible, {} confidently retired ({} rays so far)",
            num_visible,
            total_facets,
            num_retired,
            total_rays);
        const float fraction =
            options.num_rays > 0
                ? std::min(
                      1.f,
                      static_cast<float>(total_rays) / static_cast<float>(options.num_rays))
                : (total_facets > 0
                       ? static_cast<float>(num_retired) / static_cast<float>(total_facets)
                       : 1.f);
        progress.update(fraction);

        if (num_retired == total_facets) {
            lagrange::logger().info("All facets confidently visible, stopping early");
            break;
        }
        if (total_rays == prev_total_rays) {
            lagrange::logger().info("No facet rays could be cast, stopping");
            break;
        }
        prev_total_rays = total_rays;

        if (cancel != nullptr && cancel->load()) {
            lagrange::logger().info("Cancelled, using results so far");
            break;
        }
        if (options.num_rays > 0 && total_rays >= options.num_rays) break;
    }
}

template <typename Mesh>
FacetAdjacencyEdges facet_adjacency_edges(const Mesh& mesh)
{
    FacetAdjacencyEdges adjacency_edges;
    auto adjacency_mesh = mesh; // compute_facet_facet_adjacency needs a writable mesh
    const auto adjacency = compute_facet_facet_adjacency(adjacency_mesh);
    for (auto f : lagrange::range(mesh.get_num_facets())) {
        for (auto neighbor : adjacency.get_neighbors(f)) {
            if (f < neighbor) adjacency_edges.emplace_back(f, neighbor);
        }
    }
    std::sort(adjacency_edges.begin(), adjacency_edges.end());
    adjacency_edges.erase(
        std::unique(adjacency_edges.begin(), adjacency_edges.end()),
        adjacency_edges.end());
    return adjacency_edges;
}

template <typename Scalar, typename Index>
void initialize_adjacency(
    const scene::SimpleScene<Scalar, Index, 3>& source,
    span<const FacetInstanceMeasures<Index>> instances,
    AdjacencyCache& cache)
{
    std::call_once(cache.once, [&] {
        std::vector<FacetAdjacencyEdges> adjacency(source.get_num_meshes());
        std::vector<bool> computed(source.get_num_meshes(), false);
        for (const auto& instance : instances) {
            const auto mesh_index = instance.mesh_index;
            if (computed[mesh_index]) continue;
            adjacency[mesh_index] = facet_adjacency_edges(source.get_mesh(mesh_index));
            computed[mesh_index] = true;
        }
        cache.adjacency_edges = std::move(adjacency);
    });
}

template <typename Scalar, typename Index>
std::vector<FacetInstanceMeasures<Index>> extract_instance_measures(
    const OccludedFacetSampler<Scalar, Index>& sampler)
{
    std::vector<FacetInstanceMeasures<Index>> instances;
    instances.reserve(sampler.instances().size());
    for (const auto& info : sampler.instances()) {
        FacetInstanceMeasures<Index> instance{
            info.mesh_index,
            info.instance_index,
            std::vector<double>(info.num_facets)};
        for (auto f : lagrange::range(info.num_facets)) {
            instance.measures[f] = sampler.visibility_measure(info.facet_offset + f);
        }
        instances.push_back(std::move(instance));
    }
    return instances;
}

// Write per-facet measures to a named Scalar attribute on the output mesh.
template <typename Scalar, typename Mesh>
void write_measure_attribute(
    Mesh& mesh,
    const std::vector<double>& measures,
    std::string_view attribute_name)
{
    std::vector<Scalar> attribute(measures.size());
    for (auto f : lagrange::range(measures.size())) attribute[f] = static_cast<Scalar>(measures[f]);
    mesh.template create_attribute<Scalar>(
        attribute_name,
        AttributeElement::Facet,
        1,
        AttributeUsage::Scalar,
        {attribute.data(), attribute.size()});
}

// One output mesh per input instance: shared meshes may cull differently, so instancing is lost.
template <typename Scalar, typename Index>
scene::SimpleScene<Scalar, Index, 3> assemble_facet_result(
    const scene::SimpleScene<Scalar, Index, 3>& scene,
    span<const FacetInstanceMeasures<Index>> instances,
    function_ref<void(Index, SurfaceMesh<Scalar, Index>&, const std::vector<double>&)> finalize,
    bool preserve_initially_empty = false)
{
    scene::SimpleScene<Scalar, Index, 3> result;
    for (const auto& info : instances) {
        auto mesh = scene.get_mesh(info.mesh_index);
        const bool retain_empty = preserve_initially_empty && mesh.get_num_facets() == 0;
        if (mesh.get_num_facets() == 0 && !retain_empty) continue;
        finalize(info.mesh_index, mesh, info.measures);
        if (mesh.get_num_facets() == 0 && !retain_empty) continue;

        auto scene_instance = scene.get_instance(info.mesh_index, info.instance_index);
        result.add_mesh(std::move(mesh));
        scene_instance.mesh_index = result.get_num_meshes() - 1;
        result.add_instance(std::move(scene_instance));
    }
    return result;
}

// Preserve instancing: one output mesh per source mesh, shared by its instances. A source facet's
// measure is aggregated over its instances per `Policy`, then culled or annotated.
template <InstancingPolicy Policy, typename Scalar, typename Index>
scene::SimpleScene<Scalar, Index, 3> assemble_instanced(
    const scene::SimpleScene<Scalar, Index, 3>& scene,
    span<const FacetInstanceMeasures<Index>> instances,
    function_ref<void(Index, SurfaceMesh<Scalar, Index>&, const std::vector<double>&)> finalize,
    bool preserve_initially_empty = false)
{
    std::vector<std::vector<const FacetInstanceMeasures<Index>*>> by_mesh(scene.get_num_meshes());
    for (const auto& info : instances) by_mesh[info.mesh_index].push_back(&info);

    scene::SimpleScene<Scalar, Index, 3> result;
    for (Index mesh_index = 0; mesh_index < scene.get_num_meshes(); ++mesh_index) {
        const auto& infos = by_mesh[mesh_index];
        if (infos.empty()) continue;

        const auto num_facets = infos.front()->measures.size();
        std::vector<double> measures(num_facets, 0.0);
        for (const auto* info : infos) {
            for (auto local_f : lagrange::range(num_facets)) {
                const double m = info->measures[local_f];
                if constexpr (Policy == InstancingPolicy::Max) {
                    measures[local_f] = std::max(measures[local_f], m);
                } else {
                    measures[local_f] += m;
                }
            }
        }
        if constexpr (Policy == InstancingPolicy::Average) {
            for (double& measure : measures) measure /= static_cast<double>(infos.size());
        }

        auto mesh = scene.get_mesh(mesh_index);
        const bool retain_empty = preserve_initially_empty && mesh.get_num_facets() == 0;
        if (mesh.get_num_facets() == 0 && !retain_empty) continue;
        finalize(mesh_index, mesh, measures);
        if (mesh.get_num_facets() == 0 && !retain_empty) continue;

        result.add_mesh(std::move(mesh));
        const Index new_mesh_index = result.get_num_meshes() - 1;
        for (const auto* info : infos) {
            auto scene_instance = scene.get_instance(info->mesh_index, info->instance_index);
            scene_instance.mesh_index = new_mesh_index;
            result.add_instance(std::move(scene_instance));
        }
    }
    return result;
}

// Aggregate double-precision measures under `policy`, then annotate or prune each output mesh.
template <typename Scalar, typename Index>
scene::SimpleScene<Scalar, Index, 3> assemble_facets(
    const scene::SimpleScene<Scalar, Index, 3>& scene,
    span<const FacetInstanceMeasures<Index>> instances,
    InstancingPolicy policy,
    function_ref<void(Index, SurfaceMesh<Scalar, Index>&, const std::vector<double>&)> finalize,
    bool preserve_initially_empty = false)
{
    switch (policy) {
    case InstancingPolicy::FlattenInstances:
        return assemble_facet_result<Scalar, Index>(
            scene,
            instances,
            finalize,
            preserve_initially_empty);
    case InstancingPolicy::Max:
        return assemble_instanced<InstancingPolicy::Max, Scalar, Index>(
            scene,
            instances,
            finalize,
            preserve_initially_empty);
    case InstancingPolicy::Average:
        return assemble_instanced<InstancingPolicy::Average, Scalar, Index>(
            scene,
            instances,
            finalize,
            preserve_initially_empty);
    }
    return {};
}

} // namespace

template <typename Scalar, typename Index>
OccludedFacetsFilter<Scalar, Index>::Impl::Impl(
    const scene::SimpleScene<Scalar, Index, 3>& scene,
    const OccludedFacetSamplerOptions& sampler_options,
    ProgressCallback& progress,
    function_ref<bool(Index, Index)> is_occluder,
    const std::atomic_bool* cancel)
    : source(scene)
    , max_visibility_threshold(sampler_options.max_visibility_threshold)
{
    OccludedFacetSampler<Scalar, Index> sampler(source, sampler_options, is_occluder);
    run_sampler(sampler, sampler_options, progress, cancel);
    instances = extract_instance_measures(sampler);
}

template <typename Scalar, typename Index>
OccludedFacetsFilter<Scalar, Index>::OccludedFacetsFilter(
    const scene::SimpleScene<Scalar, Index, 3>& scene,
    const OccludedFacetSamplerOptions& sampler_options,
    ProgressCallback& progress,
    function_ref<bool(Index mesh_index, Index instance_index)> is_occluder,
    const std::atomic_bool* cancel)
    : m_impl(make_value_ptr<Impl>(scene, sampler_options, progress, is_occluder, cancel))
{}

template <typename Scalar, typename Index>
scene::SimpleScene<Scalar, Index, 3> OccludedFacetsFilter<Scalar, Index>::apply(
    const OccludedFacetFilterOptions& options) const
{
    la_runtime_assert(static_cast<bool>(m_impl), "OccludedFacetsFilter has been moved from");
    const auto& data = *m_impl;
    la_runtime_assert(
        std::isfinite(options.visibility_threshold) && options.visibility_threshold >= 0 &&
            options.visibility_threshold <= data.max_visibility_threshold,
        "apply visibility_threshold must be finite and in [0, max_visibility_threshold]");
    la_runtime_assert(
        !options.visibility_attribute_name || !options.keep_attribute_name ||
            *options.visibility_attribute_name != *options.keep_attribute_name,
        "Visibility and keep attribute names must be distinct");
    for (const auto& instance : data.instances) {
        const auto& mesh = data.source.get_mesh(instance.mesh_index);
        if (mesh.get_num_facets() == 0 && !options.keep_attribute_name) continue;
        for (const auto* name :
             {&options.visibility_attribute_name, &options.keep_attribute_name}) {
            if (*name) {
                la_runtime_assert(
                    !mesh.has_attribute(**name),
                    lagrange::format("Attribute '{}' already exist!", **name));
            }
        }
    }
    if (options.graphcut_smoothness) {
        la_runtime_assert(
            std::isfinite(*options.graphcut_smoothness) && *options.graphcut_smoothness >= 0,
            "smoothness must be finite and non-negative");
        if (*options.graphcut_smoothness > 0) {
            initialize_adjacency<Scalar, Index>(data.source, data.instances, *data.adjacency_cache);
        }
    }
    return assemble_facets<Scalar, Index>(
        data.source,
        data.instances,
        options.instancing,
        [&](Index mesh_index, auto& mesh, const auto& measures) {
            std::vector<uint8_t> keep;
            if (options.graphcut_smoothness && !measures.empty()) {
                span<const std::pair<uint64_t, uint64_t>> edges;
                if (*options.graphcut_smoothness > 0)
                    edges = data.adjacency_cache->adjacency_edges[mesh_index];
                keep = compute_graphcut_mask(
                    measures,
                    options.visibility_threshold,
                    edges,
                    *options.graphcut_smoothness);
            } else {
                keep.resize(measures.size());
                for (auto f : lagrange::range(measures.size())) {
                    keep[f] = measures[f] >= options.visibility_threshold ? uint8_t{1} : uint8_t{0};
                }
            }
            if (options.visibility_attribute_name) {
                write_measure_attribute<Scalar>(mesh, measures, *options.visibility_attribute_name);
            }
            if (options.keep_attribute_name) {
                mesh.template create_attribute<uint8_t>(
                    *options.keep_attribute_name,
                    AttributeElement::Facet,
                    1,
                    AttributeUsage::Scalar,
                    {keep.data(), keep.size()});
            } else {
                mesh.remove_facets([&](auto f) { return keep[f] == 0; });
            }
        },
        options.keep_attribute_name.has_value());
}

template <typename Scalar, typename Index>
std::pair<scene::SimpleScene<Scalar, Index, 3>, OccludedFacetViewErrorResult>
OccludedFacetsFilter<Scalar, Index>::apply_with_view_error(
    RenderErrorEvaluator evaluate,
    const OccludedFacetViewErrorOptions& options) const
{
    la_runtime_assert(static_cast<bool>(m_impl), "OccludedFacetsFilter has been moved from");
    validate_view_error_options(options);
    const auto& data = *m_impl;
    initialize_adjacency<Scalar, Index>(data.source, data.instances, *data.adjacency_cache);
    size_t num_facets = 0;
    for (const auto& instance : data.instances) num_facets += instance.measures.size();
    std::vector<double> scores;
    scores.reserve(num_facets);
    FacetAdjacencyEdges edges;
    for (const auto& instance : data.instances) {
        const uint64_t offset = scores.size();
        scores.insert(scores.end(), instance.measures.begin(), instance.measures.end());
        for (const auto& [first, second] :
             data.adjacency_cache->adjacency_edges[instance.mesh_index]) {
            edges.emplace_back(offset + first, offset + second);
        }
    }
    const auto solved = solve_graphcut_with_view_error(scores, edges, evaluate, options);
    const auto keep =
        compute_graphcut_mask(scores, solved.threshold, edges, solved.graphcut_smoothness);
    size_t offset = 0;
    auto result = assemble_facet_result<Scalar, Index>(
        data.source,
        data.instances,
        [&](Index, auto& mesh, const auto& measures) {
            mesh.remove_facets([&](Index f) { return keep[offset + f] == 0; });
            offset += measures.size();
        });
    return {std::move(result), solved};
}

template <typename Scalar, typename Index>
scene::SimpleScene<Scalar, Index, 3> OccludedFacetsFilter<Scalar, Index>::estimate_visibility(
    std::string_view attribute_name,
    InstancingPolicy policy) const
{
    la_runtime_assert(static_cast<bool>(m_impl), "OccludedFacetsFilter has been moved from");
    const auto& data = *m_impl;
    return assemble_facets<Scalar, Index>(
        data.source,
        data.instances,
        policy,
        [&](Index, auto& mesh, const auto& measures) {
            write_measure_attribute<Scalar>(mesh, measures, attribute_name);
        });
}

template <typename Scalar, typename Index>
scene::SimpleScene<Scalar, Index, 3> estimate_facet_visibility(
    const scene::SimpleScene<Scalar, Index, 3>& scene,
    std::string_view attribute_name,
    const RemoveOccludedFacetsOptions& options,
    ProgressCallback& progress,
    function_ref<bool(Index, Index)> is_occluder,
    const std::atomic_bool* cancel)
{
    const OccludedFacetsFilter<Scalar, Index> facet_filter(
        scene,
        options.sampler_options,
        progress,
        is_occluder,
        cancel);
    return facet_filter.estimate_visibility(attribute_name, options.instancing);
}

template <typename Scalar, typename Index>
scene::SimpleScene<Scalar, Index, 3> remove_occluded_facets(
    const scene::SimpleScene<Scalar, Index, 3>& scene,
    const RemoveOccludedFacetsOptions& options,
    ProgressCallback& progress,
    function_ref<bool(Index, Index)> is_occluder,
    const std::atomic_bool* cancel)
{
    const OccludedFacetsFilter<Scalar, Index> facet_filter(
        scene,
        options.sampler_options,
        progress,
        is_occluder,
        cancel);
    OccludedFacetFilterOptions filter_options;
    filter_options.visibility_threshold = options.sampler_options.max_visibility_threshold;
    filter_options.graphcut_smoothness = options.graphcut_smoothness;
    filter_options.instancing = options.instancing;
    return facet_filter.apply(filter_options);
}

template <typename Scalar, typename Index>
std::pair<scene::SimpleScene<Scalar, Index, 3>, OccludedFacetViewErrorResult>
remove_occluded_facets_with_view_error(
    const scene::SimpleScene<Scalar, Index, 3>& scene,
    RenderErrorEvaluator evaluate,
    const RemoveOccludedFacetsOptions& options,
    const OccludedFacetViewErrorOptions& solver_options,
    ProgressCallback& progress,
    function_ref<bool(Index, Index)> is_occluder,
    const std::atomic_bool* cancel)
{
    validate_view_error_options(solver_options);
    const OccludedFacetsFilter<Scalar, Index> facet_filter(
        scene,
        options.sampler_options,
        progress,
        is_occluder,
        cancel);
    return facet_filter.apply_with_view_error(evaluate, solver_options);
}

// clang-format off
#define LA_X_OccludedFacetsFilter(_, Scalar, Index) \
    template class LA_RAYCASTING_API OccludedFacetsFilter<Scalar, Index>;
LA_SURFACE_MESH_X(OccludedFacetsFilter, 0)

#define LA_X_estimate_facet_visibility(_, Scalar, Index)                                      \
    template LA_RAYCASTING_API scene::SimpleScene<Scalar, Index, 3>                           \
    estimate_facet_visibility(                                                                \
        const scene::SimpleScene<Scalar, Index, 3>&,                                          \
        std::string_view,                                                                     \
        const RemoveOccludedFacetsOptions&,                                                   \
        ProgressCallback&,                                                                    \
        function_ref<bool(Index, Index)>,                                                     \
        const std::atomic_bool*);
LA_SURFACE_MESH_X(estimate_facet_visibility, 0)

#define LA_X_remove_occluded_facets(_, Scalar, Index)                                         \
    template LA_RAYCASTING_API scene::SimpleScene<Scalar, Index, 3> remove_occluded_facets(   \
        const scene::SimpleScene<Scalar, Index, 3>&,                                          \
        const RemoveOccludedFacetsOptions&,                                                   \
        ProgressCallback&,                                                                    \
        function_ref<bool(Index, Index)>,                                                     \
        const std::atomic_bool*);
LA_SURFACE_MESH_X(remove_occluded_facets, 0)

#define LA_X_remove_occluded_facets_with_view_error(_, Scalar, Index)                          \
    template LA_RAYCASTING_API std::pair<scene::SimpleScene<Scalar, Index, 3>,                   \
                                         OccludedFacetViewErrorResult>                        \
    remove_occluded_facets_with_view_error(                                                   \
        const scene::SimpleScene<Scalar, Index, 3>&,                                          \
        RenderErrorEvaluator,                                                                \
        const RemoveOccludedFacetsOptions&,                                                  \
        const OccludedFacetViewErrorOptions&,                                                \
        ProgressCallback&,                                                                  \
        function_ref<bool(Index, Index)>,                                                     \
        const std::atomic_bool*);
LA_SURFACE_MESH_X(remove_occluded_facets_with_view_error, 0)

// clang-format on

} // namespace lagrange::raycasting
