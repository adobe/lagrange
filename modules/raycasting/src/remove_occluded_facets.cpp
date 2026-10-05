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

#include <lagrange/AttributeFwd.h>
#include <lagrange/Logger.h>
#include <lagrange/SurfaceMeshTypes.h>
#include <lagrange/compute_facet_facet_adjacency.h>
#include <lagrange/scene/filter_instances.h>
#include <lagrange/utils/assert.h>
#include <lagrange/utils/range.h>

#include <tbb/enumerable_thread_specific.h>
#include <tbb/parallel_for.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <numeric>
#include <string_view>
#include <vector>

namespace lagrange::raycasting {

using namespace detail;

namespace {

// 1/phi: additive low-discrepancy sequence for stratified seed selection (no <random> needed).
constexpr double k_golden = 0.6180339887498949;

} // namespace

namespace internal {

template <typename Scalar, typename Index>
struct OccludedFacetSampler<Scalar, Index>::Impl
    : ImplBase<typename OccludedFacetSampler<Scalar, Index>::Impl, Scalar>
{
    using InstanceInfo = typename OccludedFacetSampler<Scalar, Index>::InstanceInfo;

    /// Per-facet Monte-Carlo statistics.
    struct FacetStats
    {
        double sum_x = 0.0;
        double log_wealth = 0.0;

        /// Fold one visibility contribution and its betting log-wealth increment.
        void record(double x, double log_wealth_step)
        {
            sum_x += x;
            log_wealth += log_wealth_step;
        }

        /// Roll a batch's `staging` stats into the cumulative total, resetting them.
        void merge(FacetStats& staging)
        {
            sum_x += staging.sum_x;
            log_wealth += staging.log_wealth;
            staging.sum_x = 0.0;
            staging.log_wealth = 0.0;
        }
    };

    /// Anytime-valid retire-KEEP process shared by plain cosine and adaptive MIS sampling.
    struct BettingPolicy
    {
        double neg_log_alpha = -std::log(0.01);
        /// Predictable betting fraction, frozen from pre-batch statistics.
        std::vector<double> facet_lambda;

        BettingPolicy() = default;
        BettingPolicy(double alpha, uint64_t total_facets)
            : neg_log_alpha(-std::log(alpha))
            , facet_lambda(total_facets, 0.0)
        {}

        void freeze(uint64_t global_f, uint64_t num_rays, double mean_y, double var_y, double y0)
        {
            if (y0 <= 0.0 || y0 >= 1.0) {
                facet_lambda[global_f] = 0.0;
                return;
            }
            // A non-zero predictable prior lets clearly visible facets retire in the first batch.
            facet_lambda[global_f] =
                num_rays == 0 ? 0.5 : betting::compute_lambda(mean_y, var_y, y0);
        }

        double log_step(uint64_t global_f, double y, double y0) const
        {
            return betting::logwealth_step(facet_lambda[global_f], y, y0);
        }

        std::array<double, 2> bernoulli_log_steps(uint64_t global_f, double y0) const
        {
            return {log_step(global_f, 0.0, y0), log_step(global_f, 1.0, y0)};
        }

        bool retired(const FacetStats& stat, const FacetStats& staging) const
        {
            return stat.log_wealth + staging.log_wealth >= neg_log_alpha;
        }
    };

    /// Adaptive-only sampling, MIS, seed, and second-moment state. Betting is shared separately.
    struct AdaptiveSamplingPolicy
    {
        /// One component of the vMF-mixture proposal: a seed mean direction + its hit weight.
        struct Seed
        {
            Eigen::Vector3<Scalar> dir;
            Scalar weight;
        };

        size_t k = 4; // seed-guided rays per cosine ray (K)
        double kappa = 16.0; // vMF proposal concentration
        double vmf_ratio_norm = 0.0; // vMF/cosine pdf-ratio norm, the 2pi cancels
        double merge_cos = 1.0; // seed-merge threshold
        std::vector<double> sum_x2;
        std::vector<double> staging_sum_x2;

        AdaptiveSamplingPolicy() = default;

        /// Derive adaptive proposal quantities and allocate adaptive-only second moments.
        AdaptiveSamplingPolicy(const AdaptiveOptions& options, uint64_t total_facets)
            : k(options.num_adaptive_per_cosine)
            , kappa(options.vmf_kappa)
            , vmf_ratio_norm(kappa / (1.0 - std::exp(-2.0 * kappa)))
            , merge_cos(std::cos(1.0 / std::sqrt(kappa)))
            , sum_x2(total_facets, 0.0)
            , staging_sum_x2(total_facets, 0.0)
        {}

        double contribution_bound() const { return static_cast<double>(k + 1); }

        /// Collect facet's own and edge-neighbours' snapshot seeds into `out`.
        void gather_seeds(
            const FacetInstanceData<Scalar, Index>& inst,
            Index lf,
            std::vector<Seed>& out) const
        {
            out.clear();
            auto append = [&](Index f) {
                const auto& res = inst.facet_seeds_snapshot[f];
                for (int i = 0; i < res.size; ++i) out.push_back({res.dir[i], res.weight[i]});
            };
            append(lf);
            for (Index nb : inst.facet_neighbors->get_neighbors(lf)) append(nb);
        }

        /// Snapshot escape directions before a batch; sampling reads the snapshot while flush()
        /// writes the live reservoirs, avoiding races and within-batch proposal adaptation.
        void snapshot_seeds(FacetInstanceData<Scalar, Index>& inst)
        {
            inst.facet_seeds_snapshot = inst.facet_seeds;
        }

        void record_second_moment(uint64_t global_f, double x)
        {
            staging_sum_x2[global_f] += x * x;
        }

        void merge_second_moment(uint64_t global_f)
        {
            sum_x2[global_f] += staging_sum_x2[global_f];
            staging_sum_x2[global_f] = 0.0;
        }

        /// One proposal sample: 1:K cosine:seed-guided interleave, phased on the facet's cumulative
        /// ray count `ordinal` so tiny per-batch budgets still reach the 1:K ratio over its life.
        typename RaySampler<Scalar>::Sample draw(
            RaySampler<Scalar>& sampler,
            uint64_t ordinal,
            const std::vector<Seed>& seeds,
            Scalar seed_weight_total) const
        {
            if (seed_weight_total > 0 && ordinal % (k + 1) != 0) {
                // Pick a component ∝ weight via a golden-ratio scalar, sample its vMF lobe.
                double u = static_cast<double>(ordinal) * k_golden;
                u -= std::floor(u);
                double t = u * static_cast<double>(seed_weight_total);
                const Eigen::Vector3<Scalar>* mu = &seeds.back().dir;
                for (const auto& sd : seeds) {
                    t -= static_cast<double>(sd.weight);
                    if (t < 0.0) {
                        mu = &sd.dir;
                        break;
                    }
                }
                return sampler.sample_vmf(mu->transpose(), static_cast<Scalar>(kappa));
            }
            return sampler.sample_cosine();
        }

        /// MIS contribution X for an escaped ray, evaluated only against the frozen proposal seeds.
        double mis_contribution(
            const FacetInstanceData<Scalar, Index>& inst,
            Index local_f,
            const Eigen::RowVector3<Scalar>& dir,
            const std::vector<Seed>& seeds) const
        {
            const double cos_theta =
                std::abs(static_cast<double>(dir.dot(inst.ray_samplers[local_f].get_normal())));
            if (cos_theta <= 0.0) return 0.0; // grazing escape: cosine pdf is 0
            if (seeds.empty()) return 1.0; // pure-cosine draw (no vMF component)

            // vMF-mixture density (1/W) Σ w_s · vmf(dir·dir_s) over own + neighbours' snapshot seeds.
            double num = 0.0, total_w = 0.0;
            for (const auto& sd : seeds) {
                num += static_cast<double>(sd.weight) *
                       std::exp(kappa * (static_cast<double>(dir.dot(sd.dir)) - 1.0));
                total_w += static_cast<double>(sd.weight);
            }
            // MIS weight X = B / (1 + K·p2/p1); the shared 1/(2pi) sphere-pdf normalizer cancels.
            const double p2_over_p1 = vmf_ratio_norm * num / (total_w * cos_theta);
            return contribution_bound() / (1.0 + static_cast<double>(k) * p2_over_p1);
        }

        /// Save an escape for proposal construction in the next batch.
        void record_escape_direction(
            FacetInstanceData<Scalar, Index>& inst,
            Index local_f,
            const Eigen::RowVector3<Scalar>& dir) const
        {
            inst.facet_seeds[local_f].add(dir.transpose(), static_cast<Scalar>(merge_cos));
        }
    };

    struct AdaptiveScratch
    {
        std::vector<typename AdaptiveSamplingPolicy::Seed> seeds_draw;
        std::vector<typename AdaptiveSamplingPolicy::Seed> seeds_flush;
    };

    explicit Impl(uint64_t total_facets, bool adaptive)
        : m_stats(total_facets)
        , m_staging(total_facets)
        , m_num_rays_cast(total_facets)
        , m_facet_tau(total_facets, 0.0)
        , m_facet_size_factor(total_facets, 0.0)
        , m_adaptive(adaptive)
    {}

    std::vector<FacetInstanceData<Scalar, Index>> m_instances;
    std::vector<InstanceInfo> m_instance_infos; // parallel to m_instances; for the public API
    /// Cumulative per-facet statistics; mean visibility = sum_x / num_rays_cast.
    std::vector<FacetStats> m_stats;
    /// Per-batch staging; each chunk owns its facets exclusively (no write races). Merged in end_batch().
    std::vector<FacetStats> m_staging;
    std::vector<uint64_t> m_num_rays_cast;
    /// Per-facet mean-visibility threshold
    /// tau_f = threshold * (mean_face_area / area_f)^size_influence, uncapped.
    std::vector<double> m_facet_tau;
    /// Per-facet (area_f / mean_face_area)^size_influence, uncapped; the size term in
    /// visibility_measure().
    std::vector<double> m_facet_size_factor;
    /// Public keep threshold applied to visibility_measure().
    double m_threshold = 0.0;
    /// Adaptive: MIS cosine + seed-guided mixture. Off: plain cosine. Both use betting retirement.
    bool m_adaptive = false;
    BettingPolicy m_betting_policy;
    /// Adaptive-only proposal, seed, MIS, and second-moment state.
    AdaptiveSamplingPolicy m_adaptive_policy;
    /// Per-worker adaptive scratch; vector capacity is retained across chunks and batches.
    tbb::enumerable_thread_specific<AdaptiveScratch> m_adaptive_scratch;
    /// Sum of `facet_weights[lf]` over each instance's active facets; refreshed in end_batch().
    std::vector<Scalar> m_instance_active_weights;
    /// Reused ray-allocation scratch; process_instance() is called sequentially across instances.
    std::vector<uint64_t> m_ray_budgets;
    std::vector<size_t> m_chunk_ends;
    uint64_t m_num_degenerate_facets = 0;

    Scalar instance_active_weight(size_t i) const { return m_instance_active_weights[i]; }

    uint64_t num_retired() const
    {
        const uint64_t num_active = std::accumulate(
            m_instances.begin(),
            m_instances.end(),
            uint64_t{0},
            [](uint64_t count, const auto& inst) {
                return count + inst.active_local_facets.size();
            });
        return m_stats.size() - num_active - m_num_degenerate_facets;
    }

    template <bool Adaptive>
    double betting_bound() const
    {
        if constexpr (Adaptive)
            return m_adaptive_policy.contribution_bound();
        else
            return 1.0;
    }

    /// Freeze each active facet's predictable betting fraction from completed prior batches.
    template <bool Adaptive>
    void freeze_betting(const FacetInstanceData<Scalar, Index>& inst, const InstanceInfo& info)
    {
        const double B = betting_bound<Adaptive>();
        const double inv_B = 1.0 / B;
        for (Index lf : inst.active_local_facets) {
            const uint64_t global_f = info.facet_offset + lf;
            const uint64_t n = m_num_rays_cast[global_f];
            const double mean_x = n > 0 ? m_stats[global_f].sum_x / static_cast<double>(n) : 0.0;
            double var_x = mean_x * (1.0 - mean_x);
            if constexpr (Adaptive) {
                if (n > 0) {
                    var_x = std::max(
                        0.0,
                        m_adaptive_policy.sum_x2[global_f] / static_cast<double>(n) -
                            mean_x * mean_x);
                }
            }
            m_betting_policy.freeze(
                global_f,
                n,
                mean_x * inv_B,
                var_x * inv_B * inv_B,
                m_facet_tau[global_f] * inv_B);
        }
    }

    /// Anytime-valid retire-KEEP test. The point-estimate gate keeps retirement consistent with
    /// the public keep predicate; queued but unflushed rays count as zero (conservative).
    template <bool Adaptive>
    bool facet_retired(uint64_t global_f, uint64_t extra_rays) const
    {
        const double B = betting_bound<Adaptive>();
        const double sum_x = m_stats[global_f].sum_x + m_staging[global_f].sum_x;
        if (m_facet_tau[global_f] <= 0.0) return true; // public keep predicate is measure >= 0
        if (m_facet_tau[global_f] >= B) return false;
        const uint64_t num_rays = m_num_rays_cast[global_f] + extra_rays;
        if (sum_x <= static_cast<double>(num_rays) * m_facet_tau[global_f]) return false;
        return m_betting_policy.retired(m_stats[global_f], m_staging[global_f]);
    }

    /// Plain or MIS estimate of the facet's cosine-weighted escaped fraction; 0 before sampling.
    double mean_visibility(uint64_t global_f) const
    {
        const uint64_t cast = m_num_rays_cast[global_f];
        if (cast == 0) return 0.0;
        return m_stats[global_f].sum_x / static_cast<double>(cast);
    }

    /// Size-weighted visibility measure: mean_visibility * (area_f / mean_face_area)^size_influence.
    double visibility_measure(uint64_t global_f) const
    {
        return mean_visibility(global_f) * m_facet_size_factor[global_f];
    }

    /// Point-estimate keep decision on the public visibility measure.
    bool kept(uint64_t global_f) const { return visibility_measure(global_f) >= m_threshold; }

    // Resolve the runtime mode to a template arg so the per-ray loop carries no mode branch.
    void process_instance(size_t i, uint64_t rays, Scalar weight)
    {
        if (m_adaptive)
            process_instance<true>(i, rays, weight);
        else
            process_instance<false>(i, rays, weight);
    }

    template <bool Adaptive>
    void process_instance(size_t i, uint64_t instance_rays, Scalar instance_weight)
    {
        auto& inst = m_instances[i];
        const auto& info = m_instance_infos[i];
        const auto& active = inst.active_local_facets;
        const size_t num_active_facets = active.size();
        if (num_active_facets == 0 || instance_weight <= 0 || instance_rays == 0) return;

        // Cumulative-sum integer ray allocation, 1-ray-per-facet baseline. Chunks (the TBB unit)
        // group consecutive facets so no two threads share one.
        constexpr uint64_t target_rays_per_chunk = 32;
        auto& budgets = m_ray_budgets;
        auto& chunk_ends = m_chunk_ends;
        budgets.resize(num_active_facets);
        chunk_ends.clear();
        chunk_ends.reserve(num_active_facets);
        {
            const uint64_t extras =
                instance_rays > num_active_facets ? instance_rays - num_active_facets : 0;
            const double rays_per_weight = static_cast<double>(extras) / instance_weight;
            double cumsum = 0;
            uint64_t allocated = 0;
            uint64_t chunk_rays = 0;
            for (auto k : lagrange::range(num_active_facets)) {
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
            if (chunk_ends.empty() || chunk_ends.back() < num_active_facets)
                chunk_ends.push_back(num_active_facets);
        }

        la_debug_assert(
            [&] {
                std::vector<Index> sorted(active.begin(), active.end());
                std::sort(sorted.begin(), sorted.end());
                return std::adjacent_find(sorted.begin(), sorted.end()) == sorted.end();
            }(),
            "active_local_facets must contain unique indices");

        freeze_betting<Adaptive>(inst, info);
        if constexpr (Adaptive) m_adaptive_policy.snapshot_seeds(inst);

        tbb::parallel_for(size_t(0), chunk_ends.size(), [&](size_t chunk_i) {
            const size_t chunk_begin = chunk_i == 0 ? 0 : chunk_ends[chunk_i - 1];
            const size_t chunk_end = chunk_ends[chunk_i];

            RayPacket16 packet;
            std::array<size_t, RayPacket16::capacity> slot_k; // chunk-local facet per slot
            [[maybe_unused]] std::array<std::array<double, 2>, RayPacket16::capacity>
                slot_betting_steps;
            [[maybe_unused]] AdaptiveScratch* scratch = nullptr;
            if constexpr (Adaptive) scratch = &m_adaptive_scratch.local();
            [[maybe_unused]] size_t seeds_flush_k = std::numeric_limits<size_t>::max();
            [[maybe_unused]] double inv_contribution_bound = 1.0;
            if constexpr (Adaptive)
                inv_contribution_bound = 1.0 / m_adaptive_policy.contribution_bound();

            auto flush = [&]() {
                if (packet.empty()) return;
                const uint32_t mask = packet.cast(this->m_ray_caster);
                for (auto j : lagrange::range(packet.count)) {
                    const bool escaped = (mask & (1u << j)) == 0;
                    const Index local_f = active[slot_k[j]];
                    const uint64_t global_f = info.facet_offset + local_f;
                    if constexpr (Adaptive) {
                        auto& ap = m_adaptive_policy;
                        if (escaped && seeds_flush_k != slot_k[j]) {
                            ap.gather_seeds(inst, local_f, scratch->seeds_flush);
                            seeds_flush_k = slot_k[j];
                        }
                        // Evaluate against the frozen proposal, then save the escape for the next
                        // batch. Blocked rays contribute zero and do not update the reservoir.
                        double x = 0.0;
                        if (escaped) {
                            const Eigen::RowVector3<Scalar> direction =
                                packet.directions.row(j).template cast<Scalar>();
                            x = ap.mis_contribution(inst, local_f, direction, scratch->seeds_flush);
                            ap.record_escape_direction(inst, local_f, direction);
                        }
                        // Betting update on Y = X/B (blocked rays pass Y = 0). Single writer/facet.
                        m_staging[global_f].record(
                            x,
                            m_betting_policy.log_step(
                                global_f,
                                x * inv_contribution_bound,
                                m_facet_tau[global_f] * inv_contribution_bound));
                        ap.record_second_moment(global_f, x);
                    } else {
                        const double x = escaped ? 1.0 : 0.0;
                        m_staging[global_f].record(x, slot_betting_steps[j][escaped]);
                    }
                }
                packet.clear();
            };

            for (auto k : lagrange::range(chunk_begin, chunk_end)) {
                const Index local_f = active[k];
                const uint64_t global_f = info.facet_offset + local_f;
                uint64_t rays_done = 0;

                // Plain Bernoulli betting has only two possible increments. Compute them once per
                // facet visit and copy them into each packet slot, avoiding a logarithm per ray.
                [[maybe_unused]] std::array<double, 2> betting_steps;
                if constexpr (!Adaptive) {
                    betting_steps =
                        m_betting_policy.bernoulli_log_steps(global_f, m_facet_tau[global_f]);
                }

                // Proposal seeds for this facet (own + neighbours), stable for the whole batch.
                [[maybe_unused]] Scalar seed_weight_total = 0;
                if constexpr (Adaptive) {
                    m_adaptive_policy.gather_seeds(inst, local_f, scratch->seeds_draw);
                    for (const auto& sd : scratch->seeds_draw) seed_weight_total += sd.weight;
                }

                for (auto r : lagrange::range(budgets[k])) {
                    // Check once on entry and after flush(); wealth cannot change between them.
                    if ((r == 0 || packet.empty()) &&
                        facet_retired<Adaptive>(global_f, rays_done)) {
                        break;
                    }

                    const auto s = [&] {
                        if constexpr (Adaptive) {
                            return m_adaptive_policy.draw(
                                inst.ray_samplers[local_f],
                                m_num_rays_cast[global_f] + rays_done,
                                scratch->seeds_draw,
                                seed_weight_total);
                        }
                        return inst.ray_samplers[local_f].sample_cosine();
                    }();
                    const auto origin = triangle_position(inst.facet_triangles[local_f], s.bary);
                    slot_k[packet.count] = k; // count escapes against the facet under test
                    if constexpr (!Adaptive) slot_betting_steps[packet.count] = betting_steps;
                    packet.push(origin, s.direction);
                    ++rays_done;

                    if (packet.full()) flush();
                }
                m_num_rays_cast[global_f] += rays_done;
            }
            flush();
        });
    }

    template <bool Adaptive>
    void end_batch_impl()
    {
        for (auto i : lagrange::range(m_instances.size())) {
            auto& inst = m_instances[i];
            const auto& info = m_instance_infos[i];

            for (Index lf : inst.active_local_facets) {
                const uint64_t global_f = info.facet_offset + lf;
                m_stats[global_f].merge(m_staging[global_f]);
                if constexpr (Adaptive) m_adaptive_policy.merge_second_moment(global_f);
            }

            // Retire-KEEP facets confirmed above their threshold tau_f at the configured
            // confidence.
            inst.active_local_facets.erase(
                std::remove_if(
                    inst.active_local_facets.begin(),
                    inst.active_local_facets.end(),
                    [&](Index lf) {
                        const uint64_t global_f = info.facet_offset + lf;
                        return facet_retired<Adaptive>(global_f, 0);
                    }),
                inst.active_local_facets.end());
            m_instance_active_weights[i] = std::accumulate(
                inst.active_local_facets.begin(),
                inst.active_local_facets.end(),
                Scalar(0),
                [&](Scalar s, Index lf) { return s + inst.facet_weights[lf]; });
        }
    }

    void end_batch()
    {
        if (m_adaptive)
            end_batch_impl<true>();
        else
            end_batch_impl<false>();
    }
};

/// @cond LA_INTERNAL_DOCS
template <typename Scalar, typename Index>
OccludedFacetSampler<Scalar, Index>::OccludedFacetSampler(
    const scene::SimpleScene<Scalar, Index, 3>& scene,
    const OccludedFacetSamplerOptions& options,
    function_ref<bool(Index, Index)> is_occluder)
{
    la_runtime_assert(
        options.threshold >= 0 && options.threshold <= 0.5,
        "OccludedFacetSamplerOptions::threshold must be in [0, 0.5]");
    la_runtime_assert(
        options.size_influence >= 0,
        "OccludedFacetSamplerOptions::size_influence must be non-negative");
    la_runtime_assert(
        options.confidence > 0 && options.confidence < 1,
        "OccludedFacetSamplerOptions::confidence must be in (0, 1)");
    if (options.adaptive) {
        la_runtime_assert(
            options.adaptive->num_adaptive_per_cosine >= 1,
            "AdaptiveOptions::num_adaptive_per_cosine must be >= 1");
        la_runtime_assert(
            options.adaptive->vmf_kappa > 0,
            "AdaptiveOptions::vmf_kappa must be positive");
    }
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

    m_impl = make_value_ptr<Impl>(total_facets, options.adaptive.has_value());
    m_impl->m_instance_infos = std::move(infos);
    m_impl->m_threshold = options.threshold;
    const double alpha = 1.0 - options.confidence;
    m_impl->m_betting_policy = {alpha, total_facets};

    // Adaptive proposal parameters and storage, allocated only when adaptive sampling is enabled.
    if (options.adaptive) m_impl->m_adaptive_policy = {*options.adaptive, total_facets};

    m_impl->m_instances.resize(m_impl->m_instance_infos.size());
    m_impl->m_instance_active_weights.resize(m_impl->m_instance_infos.size());

    std::vector<Eigen::Vector3<Scalar>> world_vertices;
    for (auto i : lagrange::range(m_impl->m_instance_infos.size())) {
        const auto& info = m_impl->m_instance_infos[i];
        auto& inst = m_impl->m_instances[i];

        const auto& scene_instance = scene.get_instance(info.mesh_index, info.instance_index);
        const auto& mesh = scene.get_mesh(info.mesh_index);
        const Index nf = info.num_facets;
        const double total_area =
            initialize_instance_geometry(inst, scene_instance.transform, mesh, world_vertices);
        inst.facet_weights.resize(nf);
        inst.active_local_facets.reserve(nf);
        // Seed reservoirs + adjacency are adaptive-only; skip their allocation for plain cosine.
        if (options.adaptive) {
            inst.facet_seeds.assign(nf, {});
            inst.facet_seeds_snapshot.resize(nf); // pre-allocated; refilled before each batch
            auto shallow = mesh;
            inst.facet_neighbors = compute_facet_facet_adjacency(shallow);
        }

        for (auto f : lagrange::range(nf)) {
            if (inst.facet_areas[f] > 0) {
                inst.active_local_facets.push_back(f);
            } else {
                ++m_impl->m_num_degenerate_facets;
            }
        }

        const double mean_face_area = nf > 0 ? total_area / static_cast<double>(nf) : 0.0;
        Scalar total_weight = 0;
        for (auto f : lagrange::range(nf)) {
            const double a = static_cast<double>(inst.facet_areas[f]);
            const double size_factor = (a > 0 && mean_face_area > 0)
                                           ? std::pow(a / mean_face_area, options.size_influence)
                                           : 0.0;
            m_impl->m_facet_size_factor[info.facet_offset + f] = size_factor;
            m_impl->m_facet_tau[info.facet_offset + f] =
                size_factor > 0 ? options.threshold / size_factor
                                : std::numeric_limits<double>::infinity();
            // Allocate rays by statistical difficulty 1/tau_f ∝ size_factor; tiny facets ride the floor.
            inst.facet_weights[f] = static_cast<Scalar>(size_factor);
            total_weight += inst.facet_weights[f];
        }
        m_impl->m_instance_active_weights[i] = total_weight;
    }

    // Ray caster sees occluder-only instances.
    lagrange::logger().info("Building ray caster");
    auto occluder_scene = scene::filter_instances(scene, is_occluder);
    m_impl->m_ray_caster.add_scene(std::move(occluder_scene));
    m_impl->m_ray_caster.commit_updates();
}
/// @endcond

template <typename Scalar, typename Index>
OccludedFacetSampler<Scalar, Index>::~OccludedFacetSampler() = default;

template <typename Scalar, typename Index>
OccludedFacetSampler<Scalar, Index>::OccludedFacetSampler(OccludedFacetSampler&&) noexcept =
    default;

template <typename Scalar, typename Index>
OccludedFacetSampler<Scalar, Index>& OccludedFacetSampler<Scalar, Index>::operator=(
    OccludedFacetSampler&&) noexcept = default;

template <typename Scalar, typename Index>
void OccludedFacetSampler<Scalar, Index>::run_batch(uint64_t num_rays)
{
    m_impl->run_batch(num_rays);
}

template <typename Scalar, typename Index>
bool OccludedFacetSampler<Scalar, Index>::is_visible(uint64_t global_facet_index) const
{
    la_runtime_assert(global_facet_index < m_impl->m_stats.size());
    return m_impl->kept(global_facet_index);
}

template <typename Scalar, typename Index>
double OccludedFacetSampler<Scalar, Index>::visibility_measure(uint64_t global_facet_index) const
{
    la_runtime_assert(global_facet_index < m_impl->m_facet_size_factor.size());
    return m_impl->visibility_measure(global_facet_index);
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
    return m_impl->m_stats.size();
}

template <typename Scalar, typename Index>
uint64_t OccludedFacetSampler<Scalar, Index>::num_retired() const
{
    return m_impl->num_retired();
}

template <typename Scalar, typename Index>
span<const typename OccludedFacetSampler<Scalar, Index>::InstanceInfo>
OccludedFacetSampler<Scalar, Index>::instances() const
{
    return {m_impl->m_instance_infos.data(), m_impl->m_instance_infos.size()};
}

} // namespace internal

namespace {

template <typename Scalar, typename Index>
void run_sampler(
    internal::OccludedFacetSampler<Scalar, Index>& sampler,
    const OccludedFacetEstimateOptions& options,
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

// Turn a copy of the source mesh into an output mesh: with an empty `attribute_name`, remove facets
// whose measure is below `threshold`; otherwise write every facet's measure to that Scalar
// attribute.
template <typename Scalar, typename Mesh>
void finalize_output_mesh(
    Mesh& mesh,
    const std::vector<double>& measures,
    double threshold,
    std::string_view attribute_name)
{
    if (attribute_name.empty()) {
        mesh.remove_facets([&](auto local_f) { return measures[local_f] < threshold; });
        return;
    }
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
    const internal::OccludedFacetSampler<Scalar, Index>& sampler,
    double threshold,
    std::string_view attribute_name)
{
    scene::SimpleScene<Scalar, Index, 3> result;
    for (const auto& info : sampler.instances()) {
        std::vector<double> measures(info.num_facets);
        for (auto local_f : lagrange::range(info.num_facets)) {
            measures[local_f] = sampler.visibility_measure(info.facet_offset + local_f);
        }
        auto mesh = scene.get_mesh(info.mesh_index);
        finalize_output_mesh<Scalar>(mesh, measures, threshold, attribute_name);
        if (mesh.get_num_facets() == 0) continue;

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
    const internal::OccludedFacetSampler<Scalar, Index>& sampler,
    double threshold,
    std::string_view attribute_name)
{
    using InstanceInfo = typename internal::OccludedFacetSampler<Scalar, Index>::InstanceInfo;
    std::vector<std::vector<const InstanceInfo*>> by_mesh(scene.get_num_meshes());
    for (const auto& info : sampler.instances()) by_mesh[info.mesh_index].push_back(&info);

    scene::SimpleScene<Scalar, Index, 3> result;
    for (Index mesh_index = 0; mesh_index < scene.get_num_meshes(); ++mesh_index) {
        const auto& infos = by_mesh[mesh_index];
        if (infos.empty()) continue;

        const Index num_facets = infos.front()->num_facets;
        std::vector<double> measures(num_facets, 0.0);
        for (const auto* info : infos) {
            for (auto local_f : lagrange::range(num_facets)) {
                const double m = sampler.visibility_measure(info->facet_offset + local_f);
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
        finalize_output_mesh<Scalar>(mesh, measures, threshold, attribute_name);
        if (mesh.get_num_facets() == 0) continue;

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

// Build the output scene under `policy`. An empty `attribute_name` removes facets below
// `threshold`; otherwise every facet's measure is written to that attribute.
template <typename Scalar, typename Index>
scene::SimpleScene<Scalar, Index, 3> assemble_facets(
    const scene::SimpleScene<Scalar, Index, 3>& scene,
    const internal::OccludedFacetSampler<Scalar, Index>& sampler,
    InstancingPolicy policy,
    double threshold,
    std::string_view attribute_name)
{
    switch (policy) {
    case InstancingPolicy::FlattenInstances:
        return assemble_facet_result(scene, sampler, threshold, attribute_name);
    case InstancingPolicy::Max:
        return assemble_instanced<InstancingPolicy::Max>(scene, sampler, threshold, attribute_name);
    case InstancingPolicy::Average:
        return assemble_instanced<InstancingPolicy::Average>(
            scene,
            sampler,
            threshold,
            attribute_name);
    }
    return {};
}

} // namespace

template <typename Scalar, typename Index>
scene::SimpleScene<Scalar, Index, 3> estimate_occluded_facet_measures(
    const scene::SimpleScene<Scalar, Index, 3>& scene,
    std::string_view attribute_name,
    const RemoveOccludedFacetsOptions& options,
    ProgressCallback& progress,
    function_ref<bool(Index, Index)> is_occluder,
    const std::atomic_bool* cancel)
{
    internal::OccludedFacetSampler<Scalar, Index> sampler(
        scene,
        options.sampler_options,
        is_occluder);
    run_sampler(sampler, options.estimate_options, progress, cancel);
    return assemble_facets(
        scene,
        sampler,
        options.instancing,
        options.sampler_options.threshold,
        attribute_name);
}

template <typename Scalar, typename Index>
scene::SimpleScene<Scalar, Index, 3> remove_occluded_facets(
    const scene::SimpleScene<Scalar, Index, 3>& scene,
    const RemoveOccludedFacetsOptions& options,
    ProgressCallback& progress,
    function_ref<bool(Index, Index)> is_occluder,
    const std::atomic_bool* cancel)
{
    internal::OccludedFacetSampler<Scalar, Index> sampler(
        scene,
        options.sampler_options,
        is_occluder);
    run_sampler(sampler, options.estimate_options, progress, cancel);
    return assemble_facets(
        scene,
        sampler,
        options.instancing,
        options.sampler_options.threshold,
        {});
}

// clang-format off
#define LA_X_estimate_occluded_facet_measures(_, Scalar, Index)                               \
    template LA_RAYCASTING_API scene::SimpleScene<Scalar, Index, 3>                           \
    estimate_occluded_facet_measures(                                                         \
        const scene::SimpleScene<Scalar, Index, 3>&,                                          \
        std::string_view,                                                                     \
        const RemoveOccludedFacetsOptions&,                                                   \
        ProgressCallback&,                                                                    \
        function_ref<bool(Index, Index)>,                                                     \
        const std::atomic_bool*);
LA_SURFACE_MESH_X(estimate_occluded_facet_measures, 0)

#define LA_X_remove_occluded_facets(_, Scalar, Index)                                         \
    template LA_RAYCASTING_API scene::SimpleScene<Scalar, Index, 3> remove_occluded_facets(   \
        const scene::SimpleScene<Scalar, Index, 3>&,                                          \
        const RemoveOccludedFacetsOptions&,                                                   \
        ProgressCallback&,                                                                    \
        function_ref<bool(Index, Index)>,                                                     \
        const std::atomic_bool*);
LA_SURFACE_MESH_X(remove_occluded_facets, 0)

#define LA_X_OccludedFacetSampler(_, Scalar, Index)                                           \
    template class LA_RAYCASTING_API internal::OccludedFacetSampler<Scalar, Index>;
LA_SURFACE_MESH_X(OccludedFacetSampler, 0)
// clang-format on

} // namespace lagrange::raycasting
