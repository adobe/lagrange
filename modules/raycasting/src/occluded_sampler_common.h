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
#pragma once

#include <lagrange/Logger.h>
#include <lagrange/internal/constants.h>
#include <lagrange/raycasting/RayCaster.h>
#include <lagrange/scene/SimpleScene.h>
#include <lagrange/utils/AdjacencyList.h>
#include <lagrange/utils/geometry3d.h>
#include <lagrange/utils/range.h>
#include <lagrange/views.h>

#include <Eigen/Core>
#include <Eigen/Geometry>

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <numeric>
#include <optional>
#include <vector>

namespace lagrange::raycasting::detail {

enum class SamplingMode {
    /// Cosine-weighted hemisphere; records each facet's escape direction.
    Normal,
    /// Casts jittered rays along escape directions of edge-adjacent visible neighbors. Skips
    /// facets that have none.
    Adaptive,
    /// Cosine-weighted hemisphere with no escape recording — baseline for benchmarking.
    BruteForce,
};

// See https://extremelearning.com.au/unreasonable-effectiveness-of-quasirandom-sequences/
template <typename Scalar>
Eigen::Array4<Scalar> generate_vec4(const size_t i)
{
    constexpr double phi_1 = 1.1673039782614186842560459;
    constexpr double phi_2 = phi_1 * phi_1;
    constexpr double phi_3 = phi_2 * phi_1;
    constexpr double phi_4 = phi_2 * phi_2;
    // +0.5 avoids the degenerate (0,0,0,0) sample at i=0.
    const Eigen::Array4<Scalar> shifted =
        (static_cast<double>(i) / Eigen::Array4d(phi_1, phi_2, phi_3, phi_4)).cast<Scalar>() +
        Scalar(0.5);
    return shifted - shifted.floor();
}

template <typename Scalar>
struct RaySampler
{
    struct Sample
    {
        Eigen::RowVector2<Scalar> bary;
        Eigen::RowVector3<Scalar> direction;
    };

    explicit RaySampler(const Eigen::RowVector3<Scalar>& normal)
        : m_normal(normal)
    {
        Eigen::Vector3<Scalar> x_col, y_col;
        lagrange::orthogonal_frame(Eigen::Vector3<Scalar>(normal.transpose()), x_col, y_col);
        m_x_axis = x_col.transpose();
        m_y_axis = y_col.transpose();
    }

    // Atomic member blocks the implicit move ctor; load explicitly so RaySampler can live in
    // a std::vector.
    RaySampler(RaySampler&& other) noexcept
        : m_normal(other.m_normal)
        , m_x_axis(other.m_x_axis)
        , m_y_axis(other.m_y_axis)
        , m_sample_index(other.m_sample_index.load(std::memory_order_relaxed))
    {}
    RaySampler(const RaySampler&) = delete;
    RaySampler& operator=(const RaySampler&) = delete;
    RaySampler& operator=(RaySampler&&) = delete;

    const Eigen::RowVector3<Scalar>& get_normal() const { return m_normal; }

    /// Bi-hemisphere sample via Malley's method: when bary reflection fires (u+v > 1), the
    /// direction is also negated, giving symmetric upper/lower coverage from one 4D draw.
    Sample operator()()
    {
        const auto vec4 =
            generate_vec4<Scalar>(m_sample_index.fetch_add(1, std::memory_order_relaxed));
        const Scalar phi = 2 * static_cast<Scalar>(lagrange::internal::pi) * vec4[0];
        const Scalar r2 = vec4[1];
        const Scalar r = std::sqrt(r2);
        const Scalar x = r * std::cos(phi);
        const Scalar y = r * std::sin(phi);
        const Scalar z = std::sqrt(Scalar(1) - r2);
        Sample result;
        result.direction = x * m_x_axis + y * m_y_axis + z * m_normal;
        result.bary = vec4.template tail<2>();
        if (result.bary[0] + result.bary[1] > 1) {
            // Intentional: direction flip pairs with the bary fold for bi-hemisphere coverage.
            result.direction = -result.direction;
            result.bary = Eigen::RowVector2<Scalar>::Ones() - result.bary;
        }
        return result;
    }

    Sample jitter(Scalar jitter_sigma)
    {
        auto vec4 = generate_vec4<Scalar>(m_sample_index.fetch_add(1, std::memory_order_relaxed));
        // Box-Muller via (1 - u1) so that u1 == 0 (at index 0) maps to r == 0, not log(0).
        const Scalar r = jitter_sigma * std::sqrt(-2 * std::log(1 - vec4[0]));
        const Scalar phi = 2 * static_cast<Scalar>(lagrange::internal::pi) * vec4[1];
        Sample result;
        result.direction = r * (std::cos(phi) * m_x_axis + std::sin(phi) * m_y_axis);
        result.bary = vec4.template tail<2>();
        if (result.bary[0] + result.bary[1] > 1) {
            // Plain bary fold — direction is a relative offset here, no hemisphere to flip.
            result.bary = Eigen::RowVector2<Scalar>::Ones() - result.bary;
        }
        return result;
    }

private:
    const Eigen::RowVector3<Scalar> m_normal;
    Eigen::RowVector3<Scalar> m_x_axis;
    Eigen::RowVector3<Scalar> m_y_axis;
    std::atomic<size_t> m_sample_index{0};
};

/// World-space position at barycentric (u, v) of facet `f`; third weight is `1 - u - v`.
/// `vertices` is column-major (3 × num_vertices).
template <typename Scalar, typename Vertices, typename Facets, typename Index>
Eigen::RowVector3<Scalar> barycentric_position(
    const Eigen::RowVector2<Scalar>& bary,
    const Vertices& vertices,
    const Facets& facets,
    Index f)
{
    const Scalar b2 = Scalar(1) - bary(0) - bary(1);
    return (bary(0) * vertices.col(facets(f, 0)) + bary(1) * vertices.col(facets(f, 1)) +
            b2 * vertices.col(facets(f, 2)))
        .transpose();
}

/// 16-ray SIMD packet for `RayCaster::occluded16`. `push` widens to float on entry.
struct RayPacket16
{
    static constexpr size_t capacity = 16;

    RayCaster::Point16f origins;
    RayCaster::Direction16f directions;
    RayCaster::Float16 tmins;
    size_t count = 0;

    template <typename Origin, typename Direction>
    void push(const Origin& origin, const Direction& direction, float tmin = 1e-6f)
    {
        origins.row(count) = origin.template cast<float>();
        directions.row(count) = direction.template cast<float>();
        tmins[count] = tmin;
        ++count;
    }

    bool full() const { return count == capacity; }
    bool empty() const { return count == 0; }
    void clear() { count = 0; }

    uint32_t cast(const RayCaster& caster) const
    {
        return caster.occluded16(origins, directions, count, tmins);
    }

    /// Low `count` bits set — masks off undefined slots in a partial packet's cast mask.
    uint32_t occupied_mask() const { return (uint32_t{1} << count) - 1; }
};

/// Per-instance precomputation shared by both samplers.
template <typename Scalar, typename Index>
struct InstanceData
{
    Index mesh_index;
    Eigen::Transform<Scalar, 3, Eigen::Affine> transform;
    std::vector<Scalar> facet_areas; // world-space
    std::vector<RaySampler<Scalar>> ray_samplers; // world-space normals
};

/// Per-instance data for OccludedFacetSampler; gates each facet individually.
template <typename Scalar, typename Index>
struct FacetInstanceData : InstanceData<Scalar, Index>
{
    /// Per-facet ray-distribution weight = cbrt(area). cbrt softens per-facet because each
    /// facet is its own Bernoulli trial.
    std::vector<Scalar> facet_weights;
    std::vector<Index> active_local_facets;

    /// 1-ring edge-neighbors (mesh dual graph). std::optional because AdjacencyList isn't
    /// default-constructible.
    std::optional<AdjacencyList<Index>> facet_neighbors;

    /// World-space unit-length escape direction recorded the first time each facet becomes
    /// visible; std::nullopt means "not yet visible". Adaptive reads from
    /// `facet_escape_snapshot` instead so live writes here don't race.
    std::vector<std::optional<Eigen::Vector3<Scalar>>> facet_escape_directions;
    /// Pre-batch snapshot of `facet_escape_directions`. Read by Adaptive; refreshed at the
    /// start of each `process_instance`. Kept as a member to reuse the allocation.
    std::vector<std::optional<Eigen::Vector3<Scalar>>> facet_escape_snapshot;
};

template <typename Derived, typename Scalar, typename Index>
struct ImplBase
{
    scene::SimpleScene<Scalar, Index, 3> m_scene;
    RayCaster m_ray_caster;

    Derived& derived() { return static_cast<Derived&>(*this); }
    const Derived& derived() const { return static_cast<const Derived&>(*this); }

    size_t num_instances() const { return derived().m_instances.size(); }
    Index mesh_index(size_t i) const { return derived().m_instances[i].mesh_index; }
    const auto& instance_transform(size_t i) const { return derived().m_instances[i].transform; }

    Scalar compute_total_active_weight() const
    {
        const auto r = lagrange::range(num_instances());
        return std::accumulate(r.begin(), r.end(), Scalar(0), [&](Scalar s, size_t i) {
            return s + derived().instance_active_weight(i);
        });
    }

    void run_batch(uint64_t num_rays)
    {
        const Scalar total = compute_total_active_weight();
        if (total <= 0) return;

        const auto t_start = std::chrono::steady_clock::now();

        for (auto i : lagrange::range(num_instances())) {
            const Scalar inst_weight = derived().instance_active_weight(i);
            if (inst_weight <= 0) continue;

            const uint64_t inst_rays = static_cast<uint64_t>(
                std::round(static_cast<double>(num_rays) * inst_weight / total));
            if (inst_rays == 0) continue;

            const auto& mesh = m_scene.get_mesh(mesh_index(i));
            const auto vertices =
                (instance_transform(i) * vertex_view(mesh).transpose().template topRows<3>())
                    .eval();
            const auto facets = facet_view(mesh);

            derived().process_instance(i, inst_rays, inst_weight, vertices, facets);
        }

        derived().end_batch();

        const auto t_end = std::chrono::steady_clock::now();
        const auto batch_ms =
            std::chrono::duration_cast<std::chrono::milliseconds>(t_end - t_start).count();
        lagrange::logger().info("Batch: {} ms", batch_ms);
    }
};

/// Sum `is_visible(i)` and `num_rays_cast(i)` over `i in [0, count)`.
template <typename Sampler, typename Index>
std::pair<uint64_t, uint64_t> sum_progress(const Sampler& sampler, Index count)
{
    uint64_t num_visible = 0;
    uint64_t total_rays = 0;
    for (Index i = 0; i < count; ++i) {
        if (sampler.is_visible(i)) ++num_visible;
        total_rays += sampler.num_rays_cast(i);
    }
    return {num_visible, total_rays};
}

} // namespace lagrange::raycasting::detail
