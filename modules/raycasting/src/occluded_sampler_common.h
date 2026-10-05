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
#include <lagrange/scene/simple_scene_bbox.h>
#include <lagrange/utils/AdjacencyList.h>
#include <lagrange/utils/assert.h>
#include <lagrange/utils/geometry3d.h>
#include <lagrange/utils/range.h>
#include <lagrange/views.h>

#include <Eigen/Core>
#include <Eigen/Geometry>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <numeric>
#include <optional>
#include <vector>

namespace lagrange::raycasting::detail {

/// Compute the upper-tail binomial p-value P(X >= n) for X ~ Binomial(num_trials, p).
///
/// @param[in] n           Number of observed successes.
/// @param[in] num_trials  Number of Bernoulli trials.
/// @param[in] p           Null-hypothesis success probability in [0, 1).
/// @return The probability of observing at least @p n successes under the null hypothesis.
///
/// The complement-of-lower-terms implementation is cheap when @p n is small, but is not intended
/// for tiny p-values (~1e-12) where cancellation in `1 - cdf` loses precision.
inline double binomial_upper_tail(uint64_t n, uint64_t num_trials, double p)
{
    if (n == 0) return 1.0;
    if (n > num_trials) return 0.0;
    const double q = 1.0 - p;
    double pmf = std::pow(q, static_cast<double>(num_trials)); // pmf(0)
    double cdf = pmf; // P(X <= 0)
    for (uint64_t k = 1; k < n; ++k) {
        pmf *= static_cast<double>(num_trials - k + 1) / static_cast<double>(k) * (p / q);
        cdf += pmf;
    }
    return 1.0 - cdf; // P(X >= n) = 1 - P(X <= n-1)
}

/// One-sided test "is the true escape mean confidently above `threshold`?" from `n` escapes in
/// `num_trials` rays at significance `alpha`. O(1) amortized: a point-estimate pre-gate rejects
/// the bulk, exact binomial for small `num_trials`, normal approximation (erfc, no pow underflow)
/// for large `num_trials`.
inline bool is_confidently_visible(uint64_t n, uint64_t num_trials, double threshold, double alpha)
{
    if (n == 0 || num_trials == 0) return false;
    if (threshold <= 0.0) return true; // any escape clears a zero threshold
    if (threshold >= 1.0) return false; // a bounded Bernoulli mean cannot be strictly above 1
    if (static_cast<double>(n) <= static_cast<double>(num_trials) * threshold) return false;

    constexpr uint64_t exact_cap = 1000;
    if (num_trials <= exact_cap) return binomial_upper_tail(n, num_trials, threshold) <= alpha;

    const double variance = static_cast<double>(num_trials) * threshold * (1.0 - threshold);
    const double z = (static_cast<double>(n) - 0.5 - static_cast<double>(num_trials) * threshold) /
                     std::sqrt(variance);
    return 0.5 * std::erfc(z / std::sqrt(2.0)) <= alpha;
}

/// Anytime-valid betting test helpers for a bounded mean, following Waudby-Smith & Ramdas (2024),
/// Section 4 and Supplement B: https://doi.org/10.1093/jrsssb/qkad009.
namespace betting {

// Choose a predictable betting fraction and clamp it so every wealth factor remains positive.
[[nodiscard]] constexpr double compute_lambda(double running_mean, double running_var, double y0)
{
    const double lambda = (running_mean - y0) / std::max(running_var, 1e-9);
    return std::clamp(lambda, 0.0, 0.5 / std::max(y0, 1e-9));
}

// Return the log-wealth increment from one observation; blocked rays pass `y = 0`.
[[nodiscard]] inline double logwealth_step(double lambda, double y, double y0)
{
    return std::log1p(lambda * (y - y0));
}

} // namespace betting

template <typename Scalar, typename Index>
double scene_aabb_surface_area(const scene::SimpleScene<Scalar, Index, 3>& scene)
{
    const auto bbox = scene::simple_scene_bbox(scene);
    if (bbox.isEmpty()) return 0.0;
    const Eigen::Vector3<Scalar> d = bbox.sizes().cwiseMax(Scalar(0));
    return 2.0 * (static_cast<double>(d.x()) * d.y() + static_cast<double>(d.y()) * d.z() +
                  static_cast<double>(d.x()) * d.z());
}

// See https://extremelearning.com.au/unreasonable-effectiveness-of-quasirandom-sequences/
template <typename Scalar>
Eigen::Array4<Scalar> generate_vec4(const size_t i)
{
    constexpr double phi_1 = 1.1673039782614186842560459;
    constexpr double phi_2 = phi_1 * phi_1;
    constexpr double phi_3 = phi_2 * phi_1;
    constexpr double phi_4 = phi_2 * phi_2;
    const Eigen::Array4d phi(phi_1, phi_2, phi_3, phi_4);
    // +0.5 avoids the degenerate (0,0,0,0) sample at i=0.
    const Eigen::Array4d shifted = static_cast<double>(i) / phi + 0.5;
    return (shifted - shifted.floor()).template cast<Scalar>();
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
    Sample sample_cosine()
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

    /// Sample a direction from a von Mises-Fisher lobe (mean `axis`, concentration `kappa`) on the
    /// full sphere, plus a folded barycentric point.
    Sample sample_vmf(const Eigen::RowVector3<Scalar>& axis, Scalar kappa)
    {
        const auto vec4 =
            generate_vec4<Scalar>(m_sample_index.fetch_add(1, std::memory_order_relaxed));
        const double kappa_d = static_cast<double>(kappa);
        const double u = static_cast<double>(vec4[0]);
        const double arg = u + (1.0 - u) * std::exp(-2.0 * kappa_d);
        const Scalar w = static_cast<Scalar>(1.0 + std::log(arg) / kappa_d);
        const Scalar s = std::sqrt(std::max(Scalar(0), Scalar(1) - w * w));
        const Scalar phi = 2 * static_cast<Scalar>(lagrange::internal::pi) * vec4[1];
        Eigen::Vector3<Scalar> e1, e2;
        lagrange::orthogonal_frame(Eigen::Vector3<Scalar>(axis.transpose()), e1, e2);
        Sample result;
        result.direction =
            w * axis + s * (std::cos(phi) * e1.transpose() + std::sin(phi) * e2.transpose());
        result.bary = vec4.template tail<2>();
        if (result.bary[0] + result.bary[1] > 1) {
            result.bary = Eigen::RowVector2<Scalar>::Ones() - result.bary;
        }
        la_debug_assert(result.direction.allFinite(), "sample_vmf: non-finite direction");
        return result;
    }

private:
    const Eigen::RowVector3<Scalar> m_normal;
    Eigen::RowVector3<Scalar> m_x_axis;
    Eigen::RowVector3<Scalar> m_y_axis;
    std::atomic<size_t> m_sample_index{0};
};

/// Precomputed world-space facet triangle for O(1) ray origins: `base = V2`, `edge0 = V0 - V2`,
/// `edge1 = V1 - V2`, so a barycentric `(b0, b1)` maps to `base + b0*edge0 + b1*edge1`.
template <typename Scalar>
struct FacetTriangle
{
    Eigen::Vector3<Scalar> base;
    Eigen::Vector3<Scalar> edge0;
    Eigen::Vector3<Scalar> edge1;
};

/// World-space ray origin at barycentric `bary` from a precomputed triangle (no vertex gather).
template <typename Scalar>
Eigen::RowVector3<Scalar> triangle_position(
    const FacetTriangle<Scalar>& t,
    const Eigen::RowVector2<Scalar>& bary)
{
    return (t.base + bary(0) * t.edge0 + bary(1) * t.edge1).transpose();
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
template <typename Scalar>
struct InstanceData
{
    std::vector<Scalar> facet_areas; // world-space
    std::vector<RaySampler<Scalar>> ray_samplers; // world-space normals
    std::vector<FacetTriangle<Scalar>> facet_triangles; // world-space, for ray origins
};

/// Fill the world-space geometry shared by both samplers. `world_vertices` is caller-owned scratch
/// so scenes with many instances reuse its allocation. Returns the instance's total surface area.
template <typename Scalar, typename Index>
double initialize_instance_geometry(
    InstanceData<Scalar>& instance,
    const Eigen::Transform<Scalar, 3, Eigen::Affine>& transform,
    const SurfaceMesh<Scalar, Index>& mesh,
    std::vector<Eigen::Vector3<Scalar>>& world_vertices)
{
    const Index num_facets = mesh.get_num_facets();
    instance.facet_areas.resize(num_facets);
    instance.facet_triangles.resize(num_facets);
    instance.ray_samplers.clear();
    instance.ray_samplers.reserve(num_facets);

    const auto vertices = vertex_view(mesh);
    const auto facets = facet_view(mesh);
    world_vertices.resize(mesh.get_num_vertices());
    for (auto v : lagrange::range(mesh.get_num_vertices())) {
        world_vertices[v] = transform * Eigen::Vector3<Scalar>(vertices.row(v).transpose());
    }

    double total_area = 0.0;
    for (auto f : lagrange::range(num_facets)) {
        const auto& v0 = world_vertices[facets(f, 0)];
        const auto& v1 = world_vertices[facets(f, 1)];
        const auto& v2 = world_vertices[facets(f, 2)];
        const Eigen::Vector3<Scalar> edge0 = v0 - v2;
        const Eigen::Vector3<Scalar> edge1 = v1 - v2;
        instance.facet_triangles[f] = {v2, edge0, edge1};

        const Eigen::Vector3<Scalar> area_vector = Scalar(0.5) * edge0.cross(edge1);
        const Scalar area = area_vector.norm();
        instance.facet_areas[f] = area;
        total_area += static_cast<double>(area);
        // A placeholder normal keeps all per-facet arrays aligned for degenerate geometry.
        instance.ray_samplers.emplace_back(
            area > 0 ? Eigen::RowVector3<Scalar>(area_vector.transpose() / area)
                     : Eigen::RowVector3<Scalar>::UnitZ());
    }
    return total_area;
}

/// Bounded set of escape-direction "seeds" for a facet, feeding the adaptive vMF-mixture proposal.
/// Each seed is a running-mean unit direction with an integer hit-count weight. `add()` merges a
/// new escape into the nearest existing seed (cosine >= `merge_cos`), else fills a free slot, else
/// evicts the lowest-weight slot — so a few dominant holes survive while noise churns through.
template <typename Scalar>
struct SeedReservoir
{
    static constexpr int capacity = 4;
    std::array<Eigen::Vector3<Scalar>, capacity> dir{};
    std::array<Scalar, capacity> weight{}; // 0 => empty slot
    int size = 0;

    void add(const Eigen::Vector3<Scalar>& d, Scalar merge_cos)
    {
        int best = -1;
        Scalar best_cos = merge_cos;
        for (int i = 0; i < size; ++i) {
            const Scalar c = dir[i].dot(d);
            if (c >= best_cos) {
                best_cos = c;
                best = i;
            }
        }
        if (best >= 0) {
            weight[best] += 1;
            dir[best] = (dir[best] + (d - dir[best]) / weight[best]).normalized(); // running mean
        } else if (size < capacity) {
            dir[size] = d;
            weight[size] = 1;
            ++size;
        } else {
            int lo = 0;
            for (int i = 1; i < capacity; ++i)
                if (weight[i] < weight[lo]) lo = i;
            dir[lo] = d;
            weight[lo] = 1;
        }
    }
};

/// Per-instance data for OccludedFacetSampler; gates each facet individually.
template <typename Scalar, typename Index>
struct FacetInstanceData : InstanceData<Scalar>
{
    /// Per-facet ray-allocation weight ∝ 1/tau_f = (area / mean_face_area)^size_influence. Rays
    /// track statistical difficulty: low-tau (large) facets need the most to resolve their test.
    std::vector<Scalar> facet_weights;
    std::vector<Index> active_local_facets;

    std::optional<AdjacencyList<Index>> facet_neighbors;

    /// Per-facet escape-direction seeds, only updated in flush() from snapshot (to avoid race condition).
    std::vector<SeedReservoir<Scalar>> facet_seeds;
    std::vector<SeedReservoir<Scalar>> facet_seeds_snapshot;
};

template <typename Derived, typename Scalar>
struct ImplBase
{
    RayCaster m_ray_caster;

    Derived& derived() { return static_cast<Derived&>(*this); }
    const Derived& derived() const { return static_cast<const Derived&>(*this); }

    size_t num_instances() const { return derived().m_instances.size(); }
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

            derived().process_instance(i, inst_rays, inst_weight);
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
