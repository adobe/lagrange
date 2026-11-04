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

#include "OccludedInstanceSampler.h"

#include <lagrange/Logger.h>
#include <lagrange/SurfaceMeshTypes.h>
#include <lagrange/scene/filter_instances.h>
#include <lagrange/utils/assert.h>
#include <lagrange/utils/range.h>

#include <tbb/blocked_range.h>
#include <tbb/parallel_reduce.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
#include <vector>

namespace lagrange::raycasting {

using namespace detail;

template <typename Scalar, typename Index>
Index OccludedInstanceSampler<Scalar, Index>::flat(Index mesh_index, Index instance_index) const
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

template <typename Scalar, typename Index>
Scalar OccludedInstanceSampler<Scalar, Index>::instance_active_weight(size_t i) const
{
    return m_retired[i] ? Scalar(0) : m_instance_weights[i];
}

template <typename Scalar, typename Index>
double OccludedInstanceSampler<Scalar, Index>::mean_visibility(size_t i) const
{
    const uint64_t escaped = m_num_escaped_rays[i];
    const uint64_t cast = m_num_rays_cast[i];
    if (cast == 0) return 0.0;
    assert(escaped <= cast);
    return static_cast<double>(escaped) / static_cast<double>(cast);
}

template <typename Scalar, typename Index>
double OccludedInstanceSampler<Scalar, Index>::visibility_measure(size_t i) const
{
    return mean_visibility(i) * m_instance_size_factor[i];
}

template <typename Scalar, typename Index>
bool OccludedInstanceSampler<Scalar, Index>::kept(size_t i) const
{
    return visibility_measure(i) >= m_threshold;
}

template <typename Scalar, typename Index>
void OccludedInstanceSampler<Scalar, Index>::process_instance(
    size_t i,
    uint64_t instance_rays,
    Scalar /*instance_weight*/)
{
    auto& inst = m_instances[i];
    const size_t num_facets = inst.facet_areas.size();
    if (num_facets == 0 || instance_rays == 0) return;

    // Rays ∝ plain facet area (no per-facet softening — the instance is one shared Bernoulli
    // trial); some facets may get zero rays.
    const Scalar area_sum =
        std::accumulate(inst.facet_areas.begin(), inst.facet_areas.end(), Scalar(0));
    if (area_sum <= 0) return;

    // boundary[k+1] = cumulative rays through facet k; reused across instances and batches.
    // Each parallel range finds its first facet once, then walks boundary linearly.
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
                std::upper_bound(boundary.begin(), boundary.end(), first_ray) - boundary.begin() -
                1);
            for (size_t p = range.begin(); p != range.end(); ++p) {
                const uint64_t r0 = p * RayPacket16::capacity;
                const size_t count =
                    static_cast<size_t>(std::min<uint64_t>(RayPacket16::capacity, total_rays - r0));

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

template <typename Scalar, typename Index>
void OccludedInstanceSampler<Scalar, Index>::end_batch()
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

    m_retired.assign(total, 0);
    m_num_escaped_rays.assign(total, 0);
    m_num_rays_cast.assign(total, 0);
    m_alpha = 1.0 - options.confidence;
    m_threshold = options.threshold;

    // Per-instance tau_i = threshold * (R / area_i)^size_influence: size_influence 0 -> mean
    // visibility, 1 -> area-weighted visibility. Degenerate instances cannot retire early.
    const double scene_aabb_area = scene_aabb_surface_area(scene);

    m_instances.resize(total);
    m_instance_weights.resize(total);
    m_instance_size_factor.resize(total);
    m_instance_tau.resize(total, 0.0);

    const Index num_meshes = scene.get_num_meshes();
    m_mesh_offset.resize(num_meshes + 1);
    m_mesh_offset[0] = 0;
    for (auto mi : lagrange::range(num_meshes)) {
        m_mesh_offset[mi + 1] = m_mesh_offset[mi] + scene.get_num_instances(mi);
    }

    std::vector<Eigen::Vector3<Scalar>> world_vertices;
    Index global = 0;
    for (auto mi : lagrange::range(scene.get_num_meshes())) {
        for (auto ii : lagrange::range(scene.get_num_instances(mi))) {
            const auto& instance = scene.get_instance(mi, ii);
            auto& inst = m_instances[global];
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
            m_instance_weights[global] = static_cast<Scalar>(size_factor);
            m_instance_size_factor[global] = size_factor;
            m_instance_tau[global] = size_factor > 0 ? options.threshold / size_factor
                                                     : std::numeric_limits<double>::infinity();
            ++global;
        }
    }

    // Ray caster sees occluder-only instances.
    lagrange::logger().info("Building ray caster");
    auto occluder_scene = scene::filter_instances(scene, is_occluder);
    this->m_ray_caster.add_scene(std::move(occluder_scene));
    this->m_ray_caster.commit_updates();
}
/// @endcond

template <typename Scalar, typename Index>
OccludedInstanceSampler<Scalar, Index>::~OccludedInstanceSampler() = default;

template <typename Scalar, typename Index>
void OccludedInstanceSampler<Scalar, Index>::run_batch(uint64_t num_rays)
{
    detail::run_sampling_batch<Scalar>(
        num_rays,
        m_instances.size(),
        [this](size_t i) { return instance_active_weight(i); },
        [this](size_t i, uint64_t rays, Scalar weight) { process_instance(i, rays, weight); },
        [this] { end_batch(); });
}

template <typename Scalar, typename Index>
bool OccludedInstanceSampler<Scalar, Index>::is_visible(Index mesh_index, Index instance_index)
    const
{
    return kept(flat(mesh_index, instance_index));
}

template <typename Scalar, typename Index>
double OccludedInstanceSampler<Scalar, Index>::visibility_measure(
    Index mesh_index,
    Index instance_index) const
{
    return visibility_measure(flat(mesh_index, instance_index));
}

template <typename Scalar, typename Index>
uint64_t OccludedInstanceSampler<Scalar, Index>::num_rays_cast(
    Index mesh_index,
    Index instance_index) const
{
    return m_num_rays_cast[flat(mesh_index, instance_index)];
}

template <typename Scalar, typename Index>
Index OccludedInstanceSampler<Scalar, Index>::num_instances() const
{
    return static_cast<Index>(m_instances.size());
}

template <typename Scalar, typename Index>
Index OccludedInstanceSampler<Scalar, Index>::num_retired() const
{
    Index count = 0;
    for (const auto retired : m_retired)
        if (retired) ++count;
    return count;
}

template <typename Scalar, typename Index>
std::pair<Index, uint64_t> OccludedInstanceSampler<Scalar, Index>::progress() const
{
    Index num_visible = 0;
    uint64_t total_rays = 0;
    for (auto i : lagrange::range(this->num_instances())) {
        if (kept(i)) ++num_visible;
        total_rays += m_num_rays_cast[i];
    }
    return {num_visible, total_rays};
}

// clang-format off
#define LA_X_OccludedInstanceSampler(_, Scalar, Index)                                          \
    template class LA_RAYCASTING_API OccludedInstanceSampler<Scalar, Index>;
LA_SURFACE_MESH_X(OccludedInstanceSampler, 0)
// clang-format on

} // namespace lagrange::raycasting
