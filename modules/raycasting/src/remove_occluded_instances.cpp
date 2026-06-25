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

#include <lagrange/raycasting/remove_occluded_instances.h>

#include "occluded_sampler_common.h"

#include <lagrange/Logger.h>
#include <lagrange/SurfaceMeshTypes.h>
#include <lagrange/compute_area.h>
#include <lagrange/scene/filter_instances.h>
#include <lagrange/utils/assert.h>
#include <lagrange/utils/hash.h>
#include <lagrange/utils/range.h>
#include <lagrange/views.h>

#include <tbb/parallel_for.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <numeric>
#include <unordered_set>

namespace lagrange::raycasting {

using namespace detail;

template <typename Scalar, typename Index>
struct OccludedInstanceSampler<Scalar, Index>::Impl
    : ImplBase<typename OccludedInstanceSampler<Scalar, Index>::Impl, Scalar, Index>
{
    explicit Impl(Index num_instances)
        : m_is_visible(num_instances)
        , m_num_rays_cast(num_instances)
    {
        // Default-constructed atomics have unspecified value in C++17; explicit zeroing.
        for (auto& v : m_is_visible) v.store(false, std::memory_order_relaxed);
        for (auto& r : m_num_rays_cast) r.store(0, std::memory_order_relaxed);
    }

    std::vector<InstanceData<Scalar, Index>> m_instances;
    /// Inter-instance ray-distribution weight = cbrt(total area). cbrt-of-sum, not sum-of-cbrt:
    /// an instance is a single shared Bernoulli trial, so softening is applied to the instance
    /// total rather than per facet.
    std::vector<Scalar> m_instance_weights;
    std::vector<std::atomic<bool>> m_is_visible;
    std::vector<std::atomic<uint64_t>> m_num_rays_cast;

    Scalar instance_active_weight(size_t i) const
    {
        return m_is_visible[i].load(std::memory_order_relaxed) ? Scalar(0) : m_instance_weights[i];
    }

    template <typename Vertices, typename Facets>
    void process_instance(
        size_t i,
        uint64_t instance_rays,
        Scalar /*instance_weight*/,
        const Vertices& vertices,
        const Facets& facets)
    {
        auto& inst = m_instances[i];
        const size_t N = inst.facet_areas.size();
        if (N == 0 || instance_rays == 0) return;

        // Rays are distributed proportionally to plain facet area — no per-facet softening,
        // since the instance is a shared Bernoulli trial. Some facets may receive zero rays.
        const Scalar area_sum =
            std::accumulate(inst.facet_areas.begin(), inst.facet_areas.end(), Scalar(0));
        if (area_sum <= 0) return;

        // boundary[k+1] = cumulative ray count up to and including facet k.
        // packet_start_facet[p] = facet that owns the first ray of packet p; the parallel_for
        // walks boundary linearly from there (≤ packet capacity steps), no binary search.
        std::vector<uint64_t> boundary(N + 1, 0);
        std::vector<size_t> packet_start_facet;
        {
            const double rays_per_area = static_cast<double>(instance_rays) / area_sum;
            double cumsum = 0;
            size_t next_p = 0;
            for (auto k : lagrange::range(N)) {
                cumsum += inst.facet_areas[k] * rays_per_area;
                boundary[k + 1] = static_cast<uint64_t>(std::llround(cumsum));
                while (next_p * RayPacket16::capacity < boundary[k + 1]) {
                    packet_start_facet.push_back(k);
                    ++next_p;
                }
            }
        }
        const uint64_t total_rays = boundary[N];
        const size_t num_packets = packet_start_facet.size();
        if (num_packets == 0) return;

        // One packet per iteration, drawing rays from up to 16 different facets. Sampler
        // thread-safety comes from the atomic index inside RaySampler.
        tbb::parallel_for(size_t(0), num_packets, [&](size_t p) {
            if (m_is_visible[i].load(std::memory_order_relaxed)) return;

            const uint64_t r0 = p * RayPacket16::capacity;
            const size_t count =
                static_cast<size_t>(std::min<uint64_t>(RayPacket16::capacity, total_rays - r0));

            RayPacket16 packet;
            size_t f = packet_start_facet[p];
            for (auto slot : lagrange::range(count)) {
                while (r0 + slot >= boundary[f + 1]) ++f;
                const auto s = inst.ray_samplers[f]();
                const auto origin = barycentric_position(s.bary, vertices, facets, f);
                packet.push(origin, s.direction);
            }

            const uint32_t mask = packet.cast(this->m_ray_caster);
            if ((mask & packet.occupied_mask()) != packet.occupied_mask()) {
                m_is_visible[i].store(true, std::memory_order_relaxed);
            }
            m_num_rays_cast[i].fetch_add(packet.count, std::memory_order_relaxed);
        });
    }

    void end_batch() {}
};

/// @cond LA_INTERNAL_DOCS
template <typename Scalar, typename Index>
OccludedInstanceSampler<Scalar, Index>::OccludedInstanceSampler(
    const scene::SimpleScene<Scalar, Index, 3>& scene,
    function_ref<bool(Index, Index)> is_occluder)
{
    const Index total = scene.compute_num_instances();
    la_runtime_assert(total > 0, "scene has no instances");

    m_impl = make_value_ptr<Impl>(total);
    m_impl->m_scene = scene;

    m_impl->m_instances.resize(total);
    m_impl->m_instance_weights.resize(total);
    Index global = 0;
    for (auto mi : lagrange::range(m_impl->m_scene.get_num_meshes())) {
        for (auto ii : lagrange::range(m_impl->m_scene.get_num_instances(mi))) {
            const auto& instance = m_impl->m_scene.get_instance(mi, ii);
            auto& inst = m_impl->m_instances[global];
            inst.mesh_index = instance.mesh_index;
            inst.transform = instance.transform;
            const auto& mesh = m_impl->m_scene.get_mesh(inst.mesh_index);
            la_runtime_assert(
                mesh.is_triangle_mesh(),
                "OccludedInstanceSampler requires triangle meshes");
            const Index nf = mesh.get_num_facets();

            auto shallow = mesh;
            const auto area_id = compute_facet_vector_area(shallow, inst.transform);
            const auto area_view = attribute_matrix_view<Scalar>(shallow, area_id);
            inst.facet_areas.resize(nf);
            inst.ray_samplers.reserve(nf);
            for (auto f : lagrange::range(nf)) {
                const auto area_vec = area_view.row(f);
                const Scalar area_norm = area_vec.norm();
                inst.facet_areas[f] = area_norm;
                // Degenerate facets contribute zero area; emplace a placeholder normal so the
                // sampler vector stays index-aligned (no rays will ever be cast from them).
                inst.ray_samplers.emplace_back(
                    area_norm > 0 ? Eigen::RowVector3<Scalar>(area_vec / area_norm)
                                  : Eigen::RowVector3<Scalar>::UnitZ());
            }
            m_impl->m_instance_weights[global] = std::cbrt(
                std::accumulate(inst.facet_areas.begin(), inst.facet_areas.end(), Scalar(0)));
            ++global;
        }
    }

    lagrange::logger().info("Building ray caster");
    auto occluder_scene = scene::filter_instances(scene, is_occluder);
    m_impl->m_ray_caster.add_scene(std::move(occluder_scene));
    m_impl->m_ray_caster.commit_updates();
}
/// @endcond

template <typename Scalar, typename Index>
OccludedInstanceSampler<Scalar, Index>::~OccludedInstanceSampler() = default;

template <typename Scalar, typename Index>
OccludedInstanceSampler<Scalar, Index>::OccludedInstanceSampler(
    OccludedInstanceSampler&&) noexcept = default;

template <typename Scalar, typename Index>
OccludedInstanceSampler<Scalar, Index>& OccludedInstanceSampler<Scalar, Index>::operator=(
    OccludedInstanceSampler&&) noexcept = default;

template <typename Scalar, typename Index>
void OccludedInstanceSampler<Scalar, Index>::run_batch(uint64_t num_rays)
{
    m_impl->run_batch(num_rays);
}

template <typename Scalar, typename Index>
bool OccludedInstanceSampler<Scalar, Index>::is_visible(Index global_index) const
{
    la_runtime_assert(global_index < m_impl->m_is_visible.size());
    return m_impl->m_is_visible[global_index].load(std::memory_order_relaxed);
}

template <typename Scalar, typename Index>
uint64_t OccludedInstanceSampler<Scalar, Index>::num_rays_cast(Index global_index) const
{
    la_runtime_assert(global_index < m_impl->m_num_rays_cast.size());
    return m_impl->m_num_rays_cast[global_index].load(std::memory_order_relaxed);
}

template <typename Scalar, typename Index>
Index OccludedInstanceSampler<Scalar, Index>::num_instances() const
{
    return static_cast<Index>(m_impl->m_instances.size());
}

template <typename Scalar, typename Index>
void estimate_occluded_instances(
    OccludedInstanceSampler<Scalar, Index>& sampler,
    const OccludedInstanceEstimateOptions& options,
    ProgressCallback& progress,
    const std::atomic_bool* cancel)
{
    la_runtime_assert(options.batch_size > 0);
    la_runtime_assert(
        options.num_rays > 0 || cancel != nullptr || options.until_converged,
        "estimate_occluded_instances needs at least one termination condition: num_rays>0, "
        "until_converged=true, or a non-null cancel flag");

    const Index total_instances = sampler.num_instances();
    lagrange::logger().info("Searching for occluded instances");
    progress.set_section("Searching for occluded instances");

    Index prev_visible = 0;
    while (true) {
        sampler.run_batch(options.batch_size);

        // `total_rays` is the actual count from the sampler, not the budgeted batch size —
        // packets are skipped once an instance becomes visible mid-batch.
        const auto [num_visible, total_rays] = sum_progress(sampler, total_instances);
        lagrange::logger().info(
            "{}/{} instances visible ({} rays so far)",
            num_visible,
            total_instances,
            total_rays);
        const float fraction =
            options.num_rays > 0
                ? std::min(
                      1.f,
                      static_cast<float>(total_rays) / static_cast<float>(options.num_rays))
                : (total_instances > 0
                       ? static_cast<float>(num_visible) / static_cast<float>(total_instances)
                       : 1.f);
        progress.update(fraction);

        if (static_cast<Index>(num_visible) == total_instances) {
            lagrange::logger().info("All instances visible, stopping early");
            break;
        }
        if (options.until_converged && static_cast<Index>(num_visible) == prev_visible) {
            lagrange::logger().info("Converged: no new visible instances in last batch, stopping");
            break;
        }
        prev_visible = static_cast<Index>(num_visible);

        if (cancel != nullptr && cancel->load()) {
            lagrange::logger().info("Cancelled, using results so far");
            break;
        }
        if (options.num_rays > 0 && total_rays >= options.num_rays) break;
    }
}

template <typename Scalar, typename Index>
void estimate_occluded_instances(
    const scene::SimpleScene<Scalar, Index, 3>& scene,
    function_ref<void(Index mesh_index, Index instance_index)> callback,
    const OccludedInstanceEstimateOptions& options,
    ProgressCallback& progress,
    function_ref<bool(Index mesh_index, Index instance_index)> is_occluder,
    const std::atomic_bool* cancel)
{
    OccludedInstanceSampler<Scalar, Index> sampler(scene, is_occluder);
    estimate_occluded_instances(sampler, options, progress, cancel);

    Index global = 0;
    for (auto mi : lagrange::range(scene.get_num_meshes())) {
        for (auto ii : lagrange::range(scene.get_num_instances(mi))) {
            if (!sampler.is_visible(global)) callback(mi, ii);
            ++global;
        }
    }
}

template <typename Scalar, typename Index>
scene::SimpleScene<Scalar, Index, 3> remove_occluded_instances(
    const scene::SimpleScene<Scalar, Index, 3>& scene,
    const OccludedInstanceEstimateOptions& options,
    ProgressCallback& progress,
    function_ref<bool(Index mesh_index, Index instance_index)> is_occluder,
    const std::atomic_bool* cancel)
{
    std::unordered_set<std::pair<Index, Index>, lagrange::OrderedPairHash<std::pair<Index, Index>>>
        occluded;
    estimate_occluded_instances<Scalar, Index>(
        scene,
        [&](Index mi, Index ii) { occluded.emplace(mi, ii); },
        options,
        progress,
        is_occluder,
        cancel);

    auto result = scene::filter_instances(scene, [&](Index mi, Index ii) {
        return occluded.find({mi, ii}) == occluded.end();
    });

    lagrange::logger().info(
        "Filtered scene: {} meshes, {} instances",
        result.get_num_meshes(),
        result.compute_num_instances());
    return result;
}

// clang-format off
#define LA_X_OccludedInstanceSampler(_, Scalar, Index)                                          \
    template class LA_RAYCASTING_API OccludedInstanceSampler<Scalar, Index>;
LA_SURFACE_MESH_X(OccludedInstanceSampler, 0)

#define LA_X_estimate_occluded_instances(_, Scalar, Index)                                      \
    template LA_RAYCASTING_API void estimate_occluded_instances(                                \
        OccludedInstanceSampler<Scalar, Index>&,                                                \
        const OccludedInstanceEstimateOptions&,                                                 \
        ProgressCallback&,                                                                      \
        const std::atomic_bool*);
LA_SURFACE_MESH_X(estimate_occluded_instances, 0)

#define LA_X_estimate_occluded_instances_scene(_, Scalar, Index)                                \
    template LA_RAYCASTING_API void estimate_occluded_instances(                                \
        const scene::SimpleScene<Scalar, Index, 3>&,                                            \
        function_ref<void(Index, Index)>,                                                       \
        const OccludedInstanceEstimateOptions&,                                                 \
        ProgressCallback&,                                                                      \
        function_ref<bool(Index, Index)>,                                                       \
        const std::atomic_bool*);
LA_SURFACE_MESH_X(estimate_occluded_instances_scene, 0)

#define LA_X_remove_occluded_instances(_, Scalar, Index)                                        \
    template LA_RAYCASTING_API scene::SimpleScene<Scalar, Index, 3> remove_occluded_instances(  \
        const scene::SimpleScene<Scalar, Index, 3>&,                                            \
        const OccludedInstanceEstimateOptions&,                                                 \
        ProgressCallback&,                                                                      \
        function_ref<bool(Index, Index)>,                                                       \
        const std::atomic_bool*);
LA_SURFACE_MESH_X(remove_occluded_instances, 0)
// clang-format on

} // namespace lagrange::raycasting
