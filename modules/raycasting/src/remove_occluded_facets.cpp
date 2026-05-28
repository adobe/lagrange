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

#include "occluded_sampler_common.h"

#include <lagrange/Logger.h>
#include <lagrange/SurfaceMeshTypes.h>
#include <lagrange/compute_area.h>
#include <lagrange/compute_facet_facet_adjacency.h>
#include <lagrange/scene/filter_instances.h>
#include <lagrange/utils/assert.h>
#include <lagrange/utils/range.h>
#include <lagrange/views.h>

#include <tbb/enumerable_thread_specific.h>
#include <tbb/parallel_for.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <numeric>

namespace lagrange::raycasting {

using namespace detail;

template <typename Scalar, typename Index>
struct OccludedFacetSampler<Scalar, Index>::Impl
    : ImplBase<typename OccludedFacetSampler<Scalar, Index>::Impl, Scalar, Index>
{
    using InstanceInfo = typename OccludedFacetSampler<Scalar, Index>::InstanceInfo;

    /// Seed for an Adaptive ray cast: a visible neighbor and its cached escape direction.
    /// Only the direction is borrowed — rays still originate on the facet under test so the
    /// K confirmations actually attest to its visibility.
    struct AdaptiveCandidate
    {
        Index neighbor;
        Eigen::Vector3<Scalar> direction;
    };

    explicit Impl(uint64_t total_facets)
        : m_num_escaped_rays_per_facet(total_facets)
        , m_staging_escaped_rays_per_facet(total_facets)
        , m_num_rays_cast(total_facets)
    {}

    std::vector<FacetInstanceData<Scalar, Index>> m_instances;
    std::vector<InstanceInfo> m_instance_infos; // parallel to m_instances; for the public API
    /// Cumulative escape tally across completed batches; visible iff value >= threshold.
    /// Saturates at 255.
    std::vector<uint8_t> m_num_escaped_rays_per_facet;
    /// Per-batch staging. Each chunk owns its facets exclusively so writes don't race.
    /// Merged into m_num_escaped_rays_per_facet in end_batch().
    std::vector<uint8_t> m_staging_escaped_rays_per_facet;
    std::vector<uint64_t> m_num_rays_cast;
    Scalar m_jitter_sigma;
    uint8_t m_visibility_threshold;
    SamplingMode m_current_mode = SamplingMode::Normal;
    /// Sum of `facet_weights[lf]` over each instance's active facets; refreshed in end_batch().
    std::vector<Scalar> m_instance_active_weights;
    /// Per-thread scratch for Adaptive candidates; reused across batches to avoid realloc.
    tbb::enumerable_thread_specific<std::vector<AdaptiveCandidate>> m_tls_candidates;

    Scalar instance_active_weight(size_t i) const { return m_instance_active_weights[i]; }

    template <typename Vertices, typename Facets>
    void process_instance(
        size_t i,
        uint64_t instance_rays,
        Scalar instance_weight,
        const Vertices& vertices,
        const Facets& facets)
    {
        auto& inst = m_instances[i];
        const auto& info = m_instance_infos[i];
        const auto& active = inst.active_local_facets;
        const size_t N = active.size();
        if (N == 0 || instance_weight <= 0 || instance_rays == 0) return;

        // Adaptive skips facets with no visible neighbor; widen the chunk target to keep
        // packets full.
        const uint64_t target_rays_per_chunk = m_current_mode == SamplingMode::Adaptive ? 64 : 32;

        // Cumulative-sum integer ray allocation with a 1-ray-per-facet baseline. Chunks (the
        // TBB parallel unit) group consecutive facets so no two threads share a facet.
        std::vector<uint64_t> budgets(N);
        std::vector<size_t> chunk_ends;
        {
            const uint64_t extras = instance_rays > N ? instance_rays - N : 0;
            const double rays_per_weight = static_cast<double>(extras) / instance_weight;
            double cumsum = 0;
            uint64_t allocated = 0;
            uint64_t chunk_rays = 0;
            for (auto k : lagrange::range(N)) {
                cumsum += inst.facet_weights[active[k]] * rays_per_weight;
                const uint64_t target = static_cast<uint64_t>(std::llround(cumsum));
                budgets[k] = 1 + (target - allocated);
                allocated = target;
                chunk_rays += budgets[k];
                if (chunk_rays >= target_rays_per_chunk) {
                    chunk_ends.push_back(k + 1);
                    chunk_rays = 0;
                }
            }
            if (chunk_ends.empty() || chunk_ends.back() < N) chunk_ends.push_back(N);
        }

        // Adaptive reads from the snapshot while flush() writes to the live array — no race.
        // operator= reuses the existing allocation (no realloc after the first batch).
        inst.facet_escape_snapshot = inst.facet_escape_directions;

        la_debug_assert(
            [&] {
                std::vector<Index> sorted(active.begin(), active.end());
                std::sort(sorted.begin(), sorted.end());
                return std::adjacent_find(sorted.begin(), sorted.end()) == sorted.end();
            }(),
            "active_local_facets must contain unique indices");

        tbb::parallel_for(size_t(0), chunk_ends.size(), [&](size_t chunk_i) {
            const size_t chunk_begin = chunk_i == 0 ? 0 : chunk_ends[chunk_i - 1];
            const size_t chunk_end = chunk_ends[chunk_i];

            RayPacket16 packet;
            std::array<size_t, RayPacket16::capacity> slot_k; // chunk-local facet per slot

            auto flush = [&]() {
                if (packet.empty()) return;
                const uint32_t mask = packet.cast(this->m_ray_caster);
                for (auto j : lagrange::range(packet.count)) {
                    if ((mask & (1u << j)) != 0) continue;
                    const Index local_f = active[slot_k[j]];
                    const uint64_t global_f = info.facet_offset + local_f;
                    const int prev = m_num_escaped_rays_per_facet[global_f] +
                                     m_staging_escaped_rays_per_facet[global_f];
                    if (prev >= 255) continue; // saturated
                    ++m_staging_escaped_rays_per_facet[global_f];
                    if (prev == 0 && m_current_mode != SamplingMode::BruteForce) {
                        // First escape: cache direction for neighbors' adaptive batches.
                        inst.facet_escape_directions[local_f] =
                            packet.directions.row(j).template cast<Scalar>().transpose();
                    }
                }
                packet.clear();
            };

            auto& candidates = m_tls_candidates.local();

            for (auto k : lagrange::range(chunk_begin, chunk_end)) {
                const Index local_f = active[k];
                const uint64_t global_f = info.facet_offset + local_f;

                candidates.clear();
                if (m_current_mode == SamplingMode::Adaptive) {
                    for (Index neighbor : inst.facet_neighbors->get_neighbors(local_f)) {
                        if (const auto& d = inst.facet_escape_snapshot[neighbor]) {
                            candidates.push_back({neighbor, *d});
                        }
                    }
                    if (candidates.empty()) continue;
                }
                la_debug_assert(m_current_mode == SamplingMode::Adaptive || candidates.empty());
                size_t ray_counter = 0;
                uint64_t rays_done = 0;

                for ([[maybe_unused]] auto r : lagrange::range(budgets[k])) {
                    // Stop once the facet crossed K — catches both intra-budget flushes and
                    // flushes from later iterations that drained leftover rays of this facet.
                    if (m_num_escaped_rays_per_facet[global_f] +
                            m_staging_escaped_rays_per_facet[global_f] >=
                        m_visibility_threshold) {
                        break;
                    }
                    Eigen::RowVector2<Scalar> bary;
                    Eigen::RowVector3<Scalar> dir;
                    if (!candidates.empty()) {
                        const auto& candidate = candidates[ray_counter++ % candidates.size()];
                        const auto s = inst.ray_samplers[local_f].jitter(m_jitter_sigma);
                        bary = s.bary;
                        dir = (candidate.direction.transpose() + s.direction).normalized();
                        // Reject rays heading into back hemisphere — happens at sharp dihedrals.
                        // The rejected attempt still counts against the budget; we don't retry.
                        if (dir.dot(inst.ray_samplers[local_f].get_normal()) < 0) continue;
                    } else {
                        const auto s = inst.ray_samplers[local_f]();
                        bary = s.bary;
                        dir = s.direction;
                    }
                    const auto origin = barycentric_position(bary, vertices, facets, local_f);

                    slot_k[packet.count] = k; // count escapes against the facet under test
                    packet.push(origin, dir);
                    ++rays_done;

                    if (packet.full()) flush();
                }
                m_num_rays_cast[global_f] += rays_done;
            }
            flush();
        });
    }

    void end_batch()
    {
        for (auto i : lagrange::range(m_instances.size())) {
            auto& inst = m_instances[i];
            const auto& info = m_instance_infos[i];

            // Merge this batch's staging into the cumulative tally (saturating at 255).
            for (Index lf : inst.active_local_facets) {
                const uint64_t global_f = info.facet_offset + lf;
                const int total = m_num_escaped_rays_per_facet[global_f] +
                                  m_staging_escaped_rays_per_facet[global_f];
                m_num_escaped_rays_per_facet[global_f] = static_cast<uint8_t>(std::min(total, 255));
                m_staging_escaped_rays_per_facet[global_f] = 0;
            }

            inst.active_local_facets.erase(
                std::remove_if(
                    inst.active_local_facets.begin(),
                    inst.active_local_facets.end(),
                    [&](Index lf) {
                        return m_num_escaped_rays_per_facet[info.facet_offset + lf] >=
                               m_visibility_threshold;
                    }),
                inst.active_local_facets.end());
            m_instance_active_weights[i] = std::accumulate(
                inst.active_local_facets.begin(),
                inst.active_local_facets.end(),
                Scalar(0),
                [&](Scalar s, Index lf) { return s + inst.facet_weights[lf]; });
        }
    }
};

template <typename Scalar, typename Index>
OccludedFacetSampler<Scalar, Index>::OccludedFacetSampler(
    const scene::SimpleScene<Scalar, Index, 3>& scene,
    const OccludedFacetSamplerOptions& options,
    function_ref<bool(Index, Index)> is_occluder)
{
    la_runtime_assert(
        options.visibility_threshold >= 1,
        "OccludedFacetSamplerOptions::visibility_threshold must be >= 1");
    la_runtime_assert(
        options.jitter_sigma >= 0,
        "OccludedFacetSamplerOptions::jitter_sigma must be non-negative");
    la_runtime_assert(scene.compute_num_instances() > 0, "scene has no instances");

    std::vector<typename OccludedFacetSampler::InstanceInfo> infos;
    infos.reserve(scene.compute_num_instances());
    uint64_t total_facets = 0;
    for (auto mi : lagrange::range(scene.get_num_meshes())) {
        const auto& mesh = scene.get_mesh(mi);
        la_runtime_assert(mesh.is_triangle_mesh(), "OccludedFacetSampler requires triangle meshes");
        const Index nf = mesh.get_num_facets();
        for (auto ii : lagrange::range(scene.get_num_instances(mi))) {
            infos.push_back({mi, ii, total_facets, nf});
            total_facets += nf;
        }
    }
    la_runtime_assert(total_facets > 0, "scene contains no facets");

    m_impl = make_value_ptr<Impl>(total_facets);
    m_impl->m_scene = scene;
    m_impl->m_instance_infos = std::move(infos);
    m_impl->m_jitter_sigma = options.jitter_sigma;
    m_impl->m_visibility_threshold = options.visibility_threshold;

    m_impl->m_instances.resize(m_impl->m_instance_infos.size());
    m_impl->m_instance_active_weights.resize(m_impl->m_instance_infos.size());

    for (auto i : lagrange::range(m_impl->m_instance_infos.size())) {
        const auto& info = m_impl->m_instance_infos[i];
        auto& inst = m_impl->m_instances[i];

        const auto& scene_instance = scene.get_instance(info.mesh_index, info.instance_index);
        inst.mesh_index = info.mesh_index;
        inst.transform = scene_instance.transform;

        const auto& mesh = scene.get_mesh(info.mesh_index);
        const Index nf = info.num_facets;

        auto shallow = mesh;
        const auto area_id = compute_facet_vector_area(shallow, inst.transform);
        const auto area_view = attribute_matrix_view<Scalar>(shallow, area_id);

        inst.facet_areas.resize(nf);
        inst.facet_weights.resize(nf);
        inst.ray_samplers.reserve(nf);
        inst.active_local_facets.reserve(nf);
        inst.facet_escape_directions.assign(nf, std::nullopt);
        inst.facet_escape_snapshot.resize(nf); // pre-allocated; refilled before each batch
        inst.facet_neighbors = compute_facet_facet_adjacency(shallow);

        Scalar total_weight = 0;
        for (auto f : lagrange::range(nf)) {
            const auto area_vec = area_view.row(f);
            const Scalar area_norm = area_vec.norm();
            inst.facet_areas[f] = area_norm;
            inst.facet_weights[f] = std::cbrt(area_norm);
            total_weight += inst.facet_weights[f];
            if (area_norm > 0) {
                inst.active_local_facets.push_back(f);
                inst.ray_samplers.emplace_back(area_vec / area_norm);
            } else {
                // Degenerate facet: kept out of active_local_facets; sampler vector
                // keeps an index-aligned placeholder.
                inst.ray_samplers.emplace_back(Eigen::RowVector3<Scalar>::UnitZ());
            }
        }
        m_impl->m_instance_active_weights[i] = total_weight;
    }

    // Ray caster sees occluder-only instances.
    lagrange::logger().info("Building ray caster");
    auto occluder_scene = scene::filter_instances(scene, is_occluder);
    m_impl->m_ray_caster.add_scene(std::move(occluder_scene));
    m_impl->m_ray_caster.commit_updates();
}

template <typename Scalar, typename Index>
OccludedFacetSampler<Scalar, Index>::~OccludedFacetSampler() = default;

template <typename Scalar, typename Index>
OccludedFacetSampler<Scalar, Index>::OccludedFacetSampler(OccludedFacetSampler&&) noexcept =
    default;

template <typename Scalar, typename Index>
OccludedFacetSampler<Scalar, Index>& OccludedFacetSampler<Scalar, Index>::operator=(
    OccludedFacetSampler&&) noexcept = default;

template <typename Scalar, typename Index>
void OccludedFacetSampler<Scalar, Index>::run_normal_batch(uint64_t num_rays)
{
    m_impl->m_current_mode = SamplingMode::Normal;
    m_impl->run_batch(num_rays);
}

template <typename Scalar, typename Index>
void OccludedFacetSampler<Scalar, Index>::run_adaptive_batch(uint64_t num_rays)
{
    m_impl->m_current_mode = SamplingMode::Adaptive;
    m_impl->run_batch(num_rays);
}

template <typename Scalar, typename Index>
void OccludedFacetSampler<Scalar, Index>::run_brute_force_batch(uint64_t num_rays)
{
    m_impl->m_current_mode = SamplingMode::BruteForce;
    m_impl->run_batch(num_rays);
}

template <typename Scalar, typename Index>
bool OccludedFacetSampler<Scalar, Index>::is_visible(uint64_t global_facet_index) const
{
    la_runtime_assert(global_facet_index < m_impl->m_num_escaped_rays_per_facet.size());
    return m_impl->m_num_escaped_rays_per_facet[global_facet_index] >=
           m_impl->m_visibility_threshold;
}

template <typename Scalar, typename Index>
uint64_t OccludedFacetSampler<Scalar, Index>::num_rays_cast(uint64_t global_facet_index) const
{
    la_runtime_assert(global_facet_index < m_impl->m_num_rays_cast.size());
    return m_impl->m_num_rays_cast[global_facet_index];
}

template <typename Scalar, typename Index>
uint64_t OccludedFacetSampler<Scalar, Index>::num_facets() const
{
    return std::accumulate(
        m_impl->m_instance_infos.begin(),
        m_impl->m_instance_infos.end(),
        uint64_t{0},
        [](uint64_t s, const InstanceInfo& info) { return s + info.num_facets; });
}

template <typename Scalar, typename Index>
span<const typename OccludedFacetSampler<Scalar, Index>::InstanceInfo>
OccludedFacetSampler<Scalar, Index>::instances() const
{
    return {m_impl->m_instance_infos.data(), m_impl->m_instance_infos.size()};
}

template <typename Scalar, typename Index>
void estimate_occluded_facets(
    OccludedFacetSampler<Scalar, Index>& sampler,
    const OccludedFacetEstimateOptions& options,
    ProgressCallback& progress,
    const std::atomic_bool* cancel)
{
    la_runtime_assert(options.batch_size > 0);
    la_runtime_assert(
        options.num_rays > 0 || cancel != nullptr || options.until_converged,
        "estimate_occluded_facets needs at least one termination condition: num_rays>0, "
        "until_converged=true, or a non-null cancel flag");

    const uint64_t total_facets = sampler.num_facets();
    const char* mode_label = options.brute_force ? "brute force" : "normal + adaptive";
    lagrange::logger().info("Searching for occluded facets ({} mode)", mode_label);
    progress.set_section("Searching for occluded facets");

    auto run_cycle = [&] {
        if (options.brute_force) {
            sampler.run_brute_force_batch(options.batch_size);
        } else {
            sampler.run_normal_batch(options.batch_size);
            for ([[maybe_unused]] auto k : lagrange::range(options.num_adaptive_per_normal)) {
                sampler.run_adaptive_batch(options.batch_size);
            }
        }
    };

    uint64_t prev_visible = 0;
    while (true) {
        run_cycle();

        // `total_rays` is the actual count from the sampler, not the budgeted batch size —
        // adaptive batches skip neighborless facets and trace far fewer rays than allocated.
        const auto [num_visible, total_rays] = sum_progress(sampler, total_facets);
        lagrange::logger()
            .info("{}/{} facets visible ({} rays so far)", num_visible, total_facets, total_rays);
        const float fraction =
            options.num_rays > 0
                ? std::min(
                      1.f,
                      static_cast<float>(total_rays) / static_cast<float>(options.num_rays))
                : (total_facets > 0
                       ? static_cast<float>(num_visible) / static_cast<float>(total_facets)
                       : 1.f);
        progress.update(fraction);

        if (num_visible == total_facets) {
            lagrange::logger().info("All facets visible, stopping early");
            break;
        }
        if (options.until_converged && num_visible == prev_visible) {
            lagrange::logger().info("Converged: no new visible facets in last cycle, stopping");
            break;
        }
        prev_visible = num_visible;

        if (cancel != nullptr && cancel->load()) {
            lagrange::logger().info("Cancelled, using results so far");
            break;
        }
        if (options.num_rays > 0 && total_rays >= options.num_rays) break;
    }
}

template <typename Scalar, typename Index>
scene::SimpleScene<Scalar, Index, 3> remove_occluded_facets(
    const scene::SimpleScene<Scalar, Index, 3>& scene,
    const RemoveOccludedFacetsOptions& options,
    ProgressCallback& progress,
    function_ref<bool(Index, Index)> is_occluder,
    const std::atomic_bool* cancel)
{
    OccludedFacetSampler<Scalar, Index> sampler(scene, options.sampler_options, is_occluder);
    estimate_occluded_facets(sampler, options.estimate_options, progress, cancel);

    // One output mesh per input instance: shared source meshes can end up with different
    // facets culled, so input instancing cannot be preserved.
    scene::SimpleScene<Scalar, Index, 3> result;
    for (const auto& info : sampler.instances()) {
        const auto& source_mesh = scene.get_mesh(info.mesh_index);
        auto filtered = source_mesh;
        filtered.remove_facets(
            [&](Index local_f) { return !sampler.is_visible(info.facet_offset + local_f); });
        if (filtered.get_num_facets() == 0) continue;

        auto scene_instance = scene.get_instance(info.mesh_index, info.instance_index);
        result.add_mesh(std::move(filtered));
        scene_instance.mesh_index = result.get_num_meshes() - 1;
        result.add_instance(std::move(scene_instance));
    }
    return result;
}

// clang-format off
#define LA_X_estimate_occluded_facets(_, Scalar, Index)                                       \
    template LA_RAYCASTING_API void estimate_occluded_facets(                                 \
        OccludedFacetSampler<Scalar, Index>&,                                                 \
        const OccludedFacetEstimateOptions&,                                                  \
        ProgressCallback&,                                                                    \
        const std::atomic_bool*);
LA_SURFACE_MESH_X(estimate_occluded_facets, 0)

#define LA_X_remove_occluded_facets(_, Scalar, Index)                                         \
    template LA_RAYCASTING_API scene::SimpleScene<Scalar, Index, 3> remove_occluded_facets(   \
        const scene::SimpleScene<Scalar, Index, 3>&,                                          \
        const RemoveOccludedFacetsOptions&,                                                   \
        ProgressCallback&,                                                                    \
        function_ref<bool(Index, Index)>,                                                     \
        const std::atomic_bool*);
LA_SURFACE_MESH_X(remove_occluded_facets, 0)

#define LA_X_OccludedFacetSampler(_, Scalar, Index)                                           \
    template class LA_RAYCASTING_API OccludedFacetSampler<Scalar, Index>;
LA_SURFACE_MESH_X(OccludedFacetSampler, 0)
// clang-format on

} // namespace lagrange::raycasting
