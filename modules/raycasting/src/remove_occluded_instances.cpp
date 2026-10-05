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
#include <lagrange/scene/filter_instances.h>
#include <lagrange/utils/assert.h>
#include <lagrange/utils/range.h>
#include <lagrange/utils/warning.h>

#include <tbb/blocked_range.h>
#include <tbb/parallel_reduce.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <limits>
#include <numeric>
#include <utility>
#include <vector>

namespace lagrange::raycasting {

using namespace detail;

namespace {

OccludedInstanceSamplerOptions legacy_sampler_options()
{
    OccludedInstanceSamplerOptions options;
    options.threshold = std::numeric_limits<double>::min();
    return options;
}

} // namespace

namespace internal {

template <typename Scalar, typename Index>
struct OccludedInstanceSampler<Scalar, Index>::Impl
    : ImplBase<typename OccludedInstanceSampler<Scalar, Index>::Impl, Scalar>
{
    explicit Impl(Index num_instances)
        : m_retired(num_instances, 0)
        , m_num_escaped_rays(num_instances, 0)
        , m_num_rays_cast(num_instances, 0)
    {}

    std::vector<InstanceData<Scalar>> m_instances;

    /// Inter-instance ray-allocation weight ∝ 1/tau_i = (area / R)^size_influence. Rays track each
    /// instance's statistical difficulty: low-tau (large) instances need the most to resolve.
    std::vector<Scalar> m_instance_weights;

    /// Cached `(area / R)^size_influence`, used by both the public measure and ray allocation.
    std::vector<double> m_instance_size_factor;

    /// Per-instance escape-fraction threshold `tau = threshold / size_factor`.
    std::vector<double> m_instance_tau;

    /// Public keep threshold applied to visibility_measure().
    double m_threshold = 0.0;

    /// Retire-KEEP flag: set once confidently above tau (sampling then stops). Occluded instances
    /// never retire; they sample to the ray budget.
    std::vector<uint8_t> m_retired;
    std::vector<uint64_t> m_num_escaped_rays;
    std::vector<uint64_t> m_num_rays_cast;
    double m_alpha = 0.01;
    /// Prefix sum over meshes: flat instance index = m_mesh_offset[mesh] + instance.
    std::vector<Index> m_mesh_offset;
    /// Reused per-instance ray-allocation scratch; process_instance() is called sequentially.
    std::vector<uint64_t> m_ray_boundaries;

    Index flat(Index mesh_index, Index instance_index) const
    {
        const size_t mi = static_cast<size_t>(mesh_index);
        la_runtime_assert(!m_mesh_offset.empty());
        la_runtime_assert(mi < m_mesh_offset.size() - 1, "mesh_index is out of bounds");
        const Index num_mesh_instances = m_mesh_offset[mi + 1] - m_mesh_offset[mi];
        la_runtime_assert(
            instance_index < num_mesh_instances,
            "instance_index is out of bounds for the selected mesh");
        return m_mesh_offset[mi] + instance_index;
    }

    Scalar instance_active_weight(size_t i) const
    {
        return m_retired[i] ? Scalar(0) : m_instance_weights[i];
    }

    /// (number of currently-visible instances, total rays cast) across all instances.
    std::pair<Index, uint64_t> progress() const
    {
        Index num_visible = 0;
        uint64_t total_rays = 0;
        for (auto i : lagrange::range(this->num_instances())) {
            if (kept(i)) ++num_visible;
            total_rays += m_num_rays_cast[i];
        }
        return {num_visible, total_rays};
    }

    Index num_retired() const
    {
        Index count = 0;
        for (const auto retired : m_retired)
            if (retired) ++count;
        return count;
    }

    /// Estimated cosine-weighted escaped fraction of the instance, in [0, 1]; 0 before sampling.
    double mean_visibility(size_t i) const
    {
        const uint64_t escaped = m_num_escaped_rays[i];
        const uint64_t cast = m_num_rays_cast[i];
        if (cast == 0) return 0.0;
        assert(escaped <= cast);
        return static_cast<double>(escaped) / static_cast<double>(cast);
    }

    /// Size-weighted visibility measure: mean_visibility * (area / R)^size_influence.
    double visibility_measure(size_t i) const
    {
        return mean_visibility(i) * m_instance_size_factor[i];
    }

    /// Point-estimate keep decision on the public visibility measure.
    bool kept(size_t i) const { return visibility_measure(i) >= m_threshold; }

    void process_instance(size_t i, uint64_t instance_rays, Scalar /*instance_weight*/)
    {
        auto& inst = m_instances[i];
        const size_t num_facets = inst.facet_areas.size();
        if (num_facets == 0 || instance_rays == 0) return;

        // Rays ∝ plain facet area (no per-facet softening — the instance is one shared Bernoulli
        // trial); some facets may get zero rays.
        const Scalar area_sum =
            std::accumulate(inst.facet_areas.begin(), inst.facet_areas.end(), Scalar(0));
        if (area_sum <= 0) return;

        // boundary[k+1] = cumulative rays through facet k. The allocation is reused across
        // instances and batches; each parallel range finds its first facet once, then walks
        // boundary linearly.
        m_ray_boundaries.resize(num_facets + 1);
        m_ray_boundaries[0] = 0;
        {
            const double rays_per_area = static_cast<double>(instance_rays) / area_sum;
            double cumsum = 0;
            for (auto k : lagrange::range(num_facets)) {
                cumsum += inst.facet_areas[k] * rays_per_area;
                m_ray_boundaries[k + 1] = static_cast<uint64_t>(std::llround(cumsum));
            }
        }
        const auto& boundary = m_ray_boundaries;
        const uint64_t total_rays = boundary[num_facets];
        const size_t num_packets =
            total_rays == 0 ? 0 : static_cast<size_t>(1 + (total_rays - 1) / RayPacket16::capacity);
        if (num_packets == 0) return;

        // Reduce packet results locally, then publish once; the instance counters are owned by the
        // calling thread.
        const uint64_t num_escaped = tbb::parallel_reduce(
            tbb::blocked_range<size_t>(0, num_packets),
            uint64_t{0},
            [&](const tbb::blocked_range<size_t>& range, uint64_t local_escaped) {
                const uint64_t first_ray = range.begin() * RayPacket16::capacity;
                size_t f = static_cast<size_t>(
                    std::upper_bound(boundary.begin(), boundary.end(), first_ray) -
                    boundary.begin() - 1);
                for (size_t p = range.begin(); p != range.end(); ++p) {
                    const uint64_t r0 = p * RayPacket16::capacity;
                    const size_t count = static_cast<size_t>(
                        std::min<uint64_t>(RayPacket16::capacity, total_rays - r0));

                    RayPacket16 packet;
                    for (auto slot : lagrange::range(count)) {
                        while (r0 + slot >= boundary[f + 1]) ++f;
                        const auto s = inst.ray_samplers[f].sample_cosine();
                        const auto origin = triangle_position(inst.facet_triangles[f], s.bary);
                        packet.push(origin, s.direction);
                    }

                    uint32_t escaped = ~packet.cast(this->m_ray_caster) & packet.occupied_mask();
                    for (; escaped != 0; escaped &= escaped - 1) ++local_escaped;
                }
                return local_escaped;
            },
            [](uint64_t a, uint64_t b) { return a + b; });

        m_num_escaped_rays[i] += num_escaped;
        m_num_rays_cast[i] += total_rays;
    }

    void end_batch()
    {
        // Retire-KEEP instances confidently above their mean-visibility threshold tau.
        for (auto i : lagrange::range(this->num_instances())) {
            if (m_retired[i]) continue;
            if (is_confidently_visible(
                    m_num_escaped_rays[i],
                    m_num_rays_cast[i],
                    m_instance_tau[i],
                    m_alpha)) {
                m_retired[i] = true;
            }
        }
    }
};

/// @cond LA_INTERNAL_DOCS
template <typename Scalar, typename Index>
OccludedInstanceSampler<Scalar, Index>::OccludedInstanceSampler(
    const scene::SimpleScene<Scalar, Index, 3>& scene,
    const OccludedInstanceSamplerOptions& options,
    function_ref<bool(Index, Index)> is_occluder)
{
    la_runtime_assert(
        options.threshold >= 0 && options.threshold <= 0.5,
        "OccludedInstanceSamplerOptions::threshold must be in [0, 0.5]");
    la_runtime_assert(
        options.confidence > 0 && options.confidence < 1,
        "OccludedInstanceSamplerOptions::confidence must be in (0, 1)");
    la_runtime_assert(
        options.size_influence >= 0,
        "OccludedInstanceSamplerOptions::size_influence must be non-negative");
    const Index total = scene.compute_num_instances();
    la_runtime_assert(total > 0, "scene has no instances");

    m_impl = make_value_ptr<Impl>(total);
    m_impl->m_alpha = 1.0 - options.confidence;
    m_impl->m_threshold = options.threshold;

    // Per-instance tau_i = threshold * (R / area_i)^size_influence: size_influence 0 -> mean
    // visibility, 1 -> area-weighted visibility. Degenerate instances cannot retire early.
    const double scene_aabb_area = scene_aabb_surface_area(scene);

    m_impl->m_instances.resize(total);
    m_impl->m_instance_weights.resize(total);
    m_impl->m_instance_size_factor.resize(total);
    m_impl->m_instance_tau.resize(total, 0.0);

    const Index num_meshes = scene.get_num_meshes();
    m_impl->m_mesh_offset.resize(num_meshes + 1);
    m_impl->m_mesh_offset[0] = 0;
    for (auto mi : lagrange::range(num_meshes)) {
        m_impl->m_mesh_offset[mi + 1] = m_impl->m_mesh_offset[mi] + scene.get_num_instances(mi);
    }

    std::vector<Eigen::Vector3<Scalar>> world_vertices;
    Index global = 0;
    for (auto mi : lagrange::range(scene.get_num_meshes())) {
        for (auto ii : lagrange::range(scene.get_num_instances(mi))) {
            const auto& instance = scene.get_instance(mi, ii);
            auto& inst = m_impl->m_instances[global];
            const auto& mesh = scene.get_mesh(instance.mesh_index);
            la_runtime_assert(
                mesh.is_triangle_mesh(),
                "OccludedInstanceSampler requires triangle meshes");
            const double area =
                initialize_instance_geometry(inst, instance.transform, mesh, world_vertices);
            const double size_factor =
                area > 0 && scene_aabb_area > 0
                    ? std::pow(area / scene_aabb_area, options.size_influence)
                    : 0.0;
            // Allocate rays by statistical difficulty 1/tau_i proportional to size_factor.
            m_impl->m_instance_weights[global] = static_cast<Scalar>(size_factor);
            m_impl->m_instance_size_factor[global] = size_factor;
            m_impl->m_instance_tau[global] = size_factor > 0
                                                 ? options.threshold / size_factor
                                                 : std::numeric_limits<double>::infinity();
            ++global;
        }
    }

    // Ray caster sees occluder-only instances.
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
bool OccludedInstanceSampler<Scalar, Index>::is_visible(Index mesh_index, Index instance_index)
    const
{
    return m_impl->kept(m_impl->flat(mesh_index, instance_index));
}

template <typename Scalar, typename Index>
double OccludedInstanceSampler<Scalar, Index>::visibility_measure(
    Index mesh_index,
    Index instance_index) const
{
    return m_impl->visibility_measure(m_impl->flat(mesh_index, instance_index));
}

template <typename Scalar, typename Index>
uint64_t OccludedInstanceSampler<Scalar, Index>::num_rays_cast(
    Index mesh_index,
    Index instance_index) const
{
    return m_impl->m_num_rays_cast[m_impl->flat(mesh_index, instance_index)];
}

template <typename Scalar, typename Index>
Index OccludedInstanceSampler<Scalar, Index>::num_instances() const
{
    return static_cast<Index>(m_impl->m_instances.size());
}

template <typename Scalar, typename Index>
Index OccludedInstanceSampler<Scalar, Index>::num_retired() const
{
    return m_impl->num_retired();
}

template <typename Scalar, typename Index>
std::pair<Index, uint64_t> OccludedInstanceSampler<Scalar, Index>::progress() const
{
    return m_impl->progress();
}

} // namespace internal

namespace {

template <typename Scalar, typename Index>
void run_sampler(
    internal::OccludedInstanceSampler<Scalar, Index>& sampler,
    const OccludedInstanceEstimateOptions& options,
    ProgressCallback& progress,
    const std::atomic_bool* cancel)
{
    la_runtime_assert(options.batch_size > 0);
    la_runtime_assert(
        options.num_rays > 0 || cancel != nullptr || options.until_converged,
        "run_sampler needs at least one termination condition: num_rays>0, "
        "until_converged=true, or a non-null cancel flag");

    const Index total_instances = sampler.num_instances();
    lagrange::logger().info("Searching for occluded instances");
    progress.set_section("Searching for occluded instances");

    Index prev_retired = 0;
    uint64_t prev_total_rays = 0;
    bool has_previous_batch = false;
    while (true) {
        sampler.run_batch(options.batch_size);

        // This is the actual count: retired instances receive no rays, and weighted allocation may
        // differ slightly from the requested batch size.
        const auto [num_visible, total_rays] = sampler.progress();
        const Index num_retired = sampler.num_retired();
        lagrange::logger().info(
            "{}/{} instances visible, {} confidently retired ({} rays so far)",
            num_visible,
            total_instances,
            num_retired,
            total_rays);
        const float fraction =
            options.num_rays > 0
                ? std::min(
                      1.f,
                      static_cast<float>(total_rays) / static_cast<float>(options.num_rays))
                : (total_instances > 0
                       ? static_cast<float>(num_retired) / static_cast<float>(total_instances)
                       : 1.f);
        progress.update(fraction);

        if (num_retired == total_instances) {
            lagrange::logger().info("All instances confidently visible, stopping early");
            break;
        }
        if (total_rays == prev_total_rays) {
            lagrange::logger().info("No instance rays could be cast, stopping");
            break;
        }
        if (options.until_converged && has_previous_batch && num_retired == prev_retired) {
            lagrange::logger().info("Converged: no instances retired in last batch, stopping");
            break;
        }
        prev_retired = num_retired;
        prev_total_rays = total_rays;
        has_previous_batch = true;

        if (cancel != nullptr && cancel->load()) {
            lagrange::logger().info("Cancelled, using results so far");
            break;
        }
        if (options.num_rays > 0 && total_rays >= options.num_rays) break;
    }
}

} // namespace

template <typename Scalar, typename Index>
std::vector<std::vector<double>> estimate_occluded_instance_measures(
    const scene::SimpleScene<Scalar, Index, 3>& scene,
    const RemoveOccludedInstancesOptions& options,
    ProgressCallback& progress,
    function_ref<bool(Index mesh_index, Index instance_index)> is_occluder,
    const std::atomic_bool* cancel)
{
    internal::OccludedInstanceSampler<Scalar, Index> sampler(
        scene,
        options.sampler_options,
        is_occluder);
    run_sampler(sampler, options.estimate_options, progress, cancel);

    std::vector<std::vector<double>> measures(scene.get_num_meshes());
    for (auto mi : lagrange::range(scene.get_num_meshes())) {
        measures[mi].resize(scene.get_num_instances(mi));
        for (auto ii : lagrange::range(scene.get_num_instances(mi))) {
            measures[mi][ii] = sampler.visibility_measure(mi, ii);
        }
    }
    return measures;
}

template <typename Scalar, typename Index>
scene::SimpleScene<Scalar, Index, 3> remove_occluded_instances(
    const scene::SimpleScene<Scalar, Index, 3>& scene,
    const RemoveOccludedInstancesOptions& options,
    ProgressCallback& progress,
    function_ref<bool(Index mesh_index, Index instance_index)> is_occluder,
    const std::atomic_bool* cancel)
{
    internal::OccludedInstanceSampler<Scalar, Index> sampler(
        scene,
        options.sampler_options,
        is_occluder);
    run_sampler(sampler, options.estimate_options, progress, cancel);

    auto result = scene::filter_instances(scene, [&](Index mi, Index ii) {
        return sampler.is_visible(mi, ii);
    });
    lagrange::logger().info(
        "Filtered scene: {} meshes, {} instances",
        result.get_num_meshes(),
        result.compute_num_instances());
    return result;
}

// Deprecated back-compat overloads preserve the old "one escaped ray keeps the instance"
// semantics with the smallest practical positive threshold. Zero would also keep instances with
// no escapes because the public comparison is inclusive.
LA_IGNORE_DEPRECATION_WARNING_BEGIN

template <typename Scalar, typename Index>
void estimate_occluded_instances(
    const scene::SimpleScene<Scalar, Index, 3>& scene,
    function_ref<void(Index mesh_index, Index instance_index)> callback,
    const OccludedInstanceEstimateOptions& options,
    ProgressCallback& progress,
    function_ref<bool(Index mesh_index, Index instance_index)> is_occluder,
    const std::atomic_bool* cancel)
{
    internal::OccludedInstanceSampler<Scalar, Index> sampler(
        scene,
        legacy_sampler_options(),
        is_occluder);
    run_sampler(sampler, options, progress, cancel);
    for (auto mi : lagrange::range(scene.get_num_meshes())) {
        for (auto ii : lagrange::range(scene.get_num_instances(mi))) {
            if (!sampler.is_visible(mi, ii)) callback(mi, ii);
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
    return remove_occluded_instances<Scalar, Index>(
        scene,
        RemoveOccludedInstancesOptions{legacy_sampler_options(), options},
        progress,
        is_occluder,
        cancel);
}

LA_IGNORE_DEPRECATION_WARNING_END

// clang-format off
#define LA_X_OccludedInstanceSampler(_, Scalar, Index)                                          \
    template class LA_RAYCASTING_API internal::OccludedInstanceSampler<Scalar, Index>;
LA_SURFACE_MESH_X(OccludedInstanceSampler, 0)

#define LA_X_estimate_occluded_instance_measures(_, Scalar, Index)                              \
    template LA_RAYCASTING_API std::vector<std::vector<double>>                                 \
    estimate_occluded_instance_measures(                                                        \
        const scene::SimpleScene<Scalar, Index, 3>&,                                            \
        const RemoveOccludedInstancesOptions&,                                                  \
        ProgressCallback&,                                                                      \
        function_ref<bool(Index, Index)>,                                                       \
        const std::atomic_bool*);
LA_SURFACE_MESH_X(estimate_occluded_instance_measures, 0)

#define LA_X_remove_occluded_instances(_, Scalar, Index)                                        \
    template LA_RAYCASTING_API scene::SimpleScene<Scalar, Index, 3> remove_occluded_instances(  \
        const scene::SimpleScene<Scalar, Index, 3>&,                                            \
        const RemoveOccludedInstancesOptions&,                                                  \
        ProgressCallback&,                                                                      \
        function_ref<bool(Index, Index)>,                                                       \
        const std::atomic_bool*);
LA_SURFACE_MESH_X(remove_occluded_instances, 0)

LA_IGNORE_DEPRECATION_WARNING_BEGIN
#define LA_X_estimate_occluded_instances_deprecated(_, Scalar, Index)                           \
    template LA_RAYCASTING_API void estimate_occluded_instances(                                \
        const scene::SimpleScene<Scalar, Index, 3>&,                                            \
        function_ref<void(Index, Index)>,                                                       \
        const OccludedInstanceEstimateOptions&,                                                 \
        ProgressCallback&,                                                                      \
        function_ref<bool(Index, Index)>,                                                       \
        const std::atomic_bool*);
LA_SURFACE_MESH_X(estimate_occluded_instances_deprecated, 0)

#define LA_X_remove_occluded_instances_deprecated(_, Scalar, Index)                             \
    template LA_RAYCASTING_API scene::SimpleScene<Scalar, Index, 3> remove_occluded_instances(  \
        const scene::SimpleScene<Scalar, Index, 3>&,                                            \
        const OccludedInstanceEstimateOptions&,                                                 \
        ProgressCallback&,                                                                      \
        function_ref<bool(Index, Index)>,                                                       \
        const std::atomic_bool*);
LA_SURFACE_MESH_X(remove_occluded_instances_deprecated, 0)
LA_IGNORE_DEPRECATION_WARNING_END
// clang-format on

} // namespace lagrange::raycasting
