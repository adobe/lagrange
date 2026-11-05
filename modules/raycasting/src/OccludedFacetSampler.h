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

#include "occluded_sampler_common.h"

#include <lagrange/raycasting/RayCaster.h>
#include <lagrange/raycasting/api.h>
#include <lagrange/raycasting/remove_occluded_facets.h>
#include <lagrange/scene/SimpleScene.h>
#include <lagrange/utils/function_ref.h>
#include <lagrange/utils/span.h>

#include <tbb/enumerable_thread_specific.h>

#include <array>
#include <cstdint>
#include <vector>

namespace lagrange::raycasting {

///
/// Stateful algorithm for finding occluded facets across every instance of a scene. Each call to
/// @ref run_batch distributes rays over facets not yet decided, progressively improving the result.
/// Per-facet results are addressed by a global facet index (mapped back via @ref instances).
///
/// The sampling strategy is fixed at construction by OccludedFacetSamplerOptions::adaptive: plain
/// cosine-weighted hemisphere sampling, or adaptive sampling that reuses escape directions to find
/// small holes faster.
///
/// @note Only 3D scenes are supported. Meshes must be triangle meshes.
///
/// @tparam Scalar  Mesh scalar type.
/// @tparam Index   Mesh index type.
///
template <typename Scalar, typename Index>
class LA_RAYCASTING_API OccludedFacetSampler
{
public:
    ///
    /// Maps a global facet index back to (mesh, instance, local facet).
    ///
    struct InstanceInfo
    {
        Index mesh_index;
        Index instance_index;
        /// Global index of this instance's first facet in the flat arrays (is_visible, etc.).
        uint64_t facet_offset;
        /// Number of facets in this instance's mesh.
        Index num_facets;
    };

    ///
    /// Build the ray caster and per-instance world-space facet data.
    ///
    /// @param[in]  scene         Scene to process. Every referenced mesh must be a triangle mesh.
    /// @param[in]  options       Sampler options.
    /// @param[in]  is_occluder   Returns whether `(mesh_index, instance_index)` should block
    ///                           rays. Non-occluder instances are still tested for visibility
    ///                           but do not contribute to the ray-caster scene.
    ///
    explicit OccludedFacetSampler(
        const scene::SimpleScene<Scalar, Index, 3>& scene,
        const OccludedFacetSamplerOptions& options = {},
        function_ref<bool(Index mesh_index, Index instance_index)> is_occluder = [](Index, Index) {
            return true;
        });

    ~OccludedFacetSampler();
    OccludedFacetSampler(const OccludedFacetSampler&) = delete;
    OccludedFacetSampler(OccludedFacetSampler&&) noexcept = default;
    OccludedFacetSampler& operator=(const OccludedFacetSampler&) = delete;
    OccludedFacetSampler& operator=(OccludedFacetSampler&&) noexcept = default;

    /// Trace one batch of `num_rays` rays, distributed across facets not yet decided.
    void run_batch(uint64_t num_rays);

    /// Whether the facet is currently kept, i.e. `visibility_measure >= threshold`.
    [[nodiscard]] bool is_visible(uint64_t global_facet_index) const;

    /// The facet's visibility measure
    /// `mean_visibility * (area_f / mean_face_area)^size_influence`, where `mean_visibility` is
    /// the plain or MIS estimate of the cosine-weighted escaped fraction.
    [[nodiscard]] double visibility_measure(uint64_t global_facet_index) const;

    /// Rays cast so far for the facet.
    [[nodiscard]] uint64_t num_rays_cast(uint64_t global_facet_index) const;

    /// Total number of facets across all instances. Multi-instance meshes are counted once
    /// per instance.
    [[nodiscard]] uint64_t num_facets() const;

    /// Number of facets confidently above the keep threshold and retired from sampling. This is
    /// useful to progressive callers for reporting and detecting that all facets have retired.
    [[nodiscard]] uint64_t num_retired() const;

    /// Per-instance metadata for mapping global facet indices to (mesh, instance, local facet).
    [[nodiscard]] span<const InstanceInfo> instances() const;

private:
    RayCaster m_ray_caster;

    /// Per-facet Monte-Carlo statistics.
    struct FacetStats
    {
        double sum_x = 0.0;
        double log_wealth = 0.0;

        /// Fold one visibility contribution and its betting log-wealth increment.
        void record(double x, double log_wealth_step);

        /// Roll a batch's `staging` stats into the cumulative total, resetting them.
        void merge(FacetStats& staging);
    };

    /// Anytime-valid retire-KEEP process shared by plain cosine and adaptive MIS sampling.
    struct BettingPolicy
    {
        double neg_log_alpha = -std::log(0.01);
        /// Predictable betting fraction, frozen from pre-batch statistics.
        std::vector<double> facet_lambda;

        BettingPolicy() = default;
        BettingPolicy(double alpha, uint64_t total_facets);

        void freeze(uint64_t global_f, uint64_t num_rays, double mean_y, double var_y, double y0);

        double log_step(uint64_t global_f, double y, double y0) const;

        std::array<double, 2> bernoulli_log_steps(uint64_t global_f, double y0) const;

        bool retired(const FacetStats& stat, const FacetStats& staging) const;
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
        AdaptiveSamplingPolicy(const AdaptiveOptions& options, uint64_t total_facets);

        double contribution_bound() const;

        /// Collect facet's own and edge-neighbours' snapshot seeds into `out`.
        void gather_seeds(
            const detail::FacetInstanceData<Scalar, Index>& inst,
            Index lf,
            std::vector<Seed>& out) const;

        /// Snapshot escape directions before a batch; sampling reads the snapshot while flush()
        /// writes the live reservoirs, avoiding races and within-batch proposal adaptation.
        void snapshot_seeds(detail::FacetInstanceData<Scalar, Index>& inst);

        void record_second_moment(uint64_t global_f, double x);

        void merge_second_moment(uint64_t global_f);

        /// One proposal sample: 1:K cosine:seed-guided interleave, phased on the facet's cumulative
        /// ray count `ordinal` so tiny per-batch budgets still reach the 1:K ratio over its life.
        typename detail::RaySampler<Scalar>::Sample draw(
            detail::RaySampler<Scalar>& sampler,
            uint64_t ordinal,
            const std::vector<Seed>& seeds,
            Scalar seed_weight_total) const;

        /// MIS contribution X for an escaped ray, evaluated only against the frozen proposal seeds.
        double mis_contribution(
            const detail::FacetInstanceData<Scalar, Index>& inst,
            Index local_f,
            const Eigen::RowVector3<Scalar>& dir,
            const std::vector<Seed>& seeds) const;

        /// Save an escape for proposal construction in the next batch.
        void record_escape_direction(
            detail::FacetInstanceData<Scalar, Index>& inst,
            Index local_f,
            const Eigen::RowVector3<Scalar>& dir) const;
    };

    struct AdaptiveScratch
    {
        std::vector<typename AdaptiveSamplingPolicy::Seed> seeds_draw;
        std::vector<typename AdaptiveSamplingPolicy::Seed> seeds_flush;
    };

    std::vector<detail::FacetInstanceData<Scalar, Index>> m_instances;
    std::vector<InstanceInfo> m_instance_infos; // parallel to m_instances; for sampler callers
    /// Cumulative per-facet statistics; mean visibility = sum_x / num_rays_cast.
    std::vector<FacetStats> m_stats;
    /// Per-batch staging; each chunk owns its facets exclusively (no write races). Merged in end_batch().
    std::vector<FacetStats> m_staging;
    std::vector<uint64_t> m_num_rays_cast;
    /// Per-facet mean-visibility threshold
    /// tau_f = max_visibility_threshold * (mean_face_area / area_f)^size_influence, uncapped.
    std::vector<double> m_facet_tau;
    /// Per-facet (area_f / mean_face_area)^size_influence, uncapped; the size term in
    /// visibility_measure().
    std::vector<double> m_facet_size_factor;
    /// Sampling retirement cutoff applied to the size-weighted visibility_measure().
    double m_max_visibility_threshold = 0.0;
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

    Scalar instance_active_weight(size_t i) const;

    template <bool Adaptive>
    double betting_bound() const;

    /// Freeze each active facet's predictable betting fraction from completed prior batches.
    template <bool Adaptive>
    void freeze_betting(
        const detail::FacetInstanceData<Scalar, Index>& inst,
        const InstanceInfo& info);

    /// Anytime-valid retire-KEEP test. The point-estimate gate keeps retirement consistent with
    /// the public keep predicate; queued but unflushed rays count as zero (conservative).
    template <bool Adaptive>
    bool facet_retired(uint64_t global_f, uint64_t extra_rays) const;

    /// Plain or MIS estimate of the facet's cosine-weighted escaped fraction; 0 before sampling.
    double mean_visibility(uint64_t global_f) const;

    /// Point-estimate keep decision on the public visibility measure.
    bool kept(uint64_t global_f) const;

    // Resolve the runtime mode to a template arg so the per-ray loop carries no mode branch.
    void process_instance(size_t i, uint64_t rays, Scalar weight);

    template <bool Adaptive>
    void process_instance(size_t i, uint64_t instance_rays, Scalar instance_weight);

    template <bool Adaptive>
    void end_batch_impl();

    void end_batch();
};

} // namespace lagrange::raycasting
