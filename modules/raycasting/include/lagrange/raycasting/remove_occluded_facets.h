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

#include <lagrange/raycasting/api.h>
#include <lagrange/scene/SimpleScene.h>
#include <lagrange/utils/ProgressCallback.h>
#include <lagrange/utils/function_ref.h>
#include <lagrange/utils/span.h>
#include <lagrange/utils/value_ptr.h>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace lagrange::raycasting {

///
/// @addtogroup module-raycasting
/// @{
///

///
/// Options for adaptive occluded-facet sampling.
///
struct AdaptiveOptions
{
    /// Seed-guided rays per cosine ray in the adaptive mixture (1:K ratio, K >= 1).
    size_t num_adaptive_per_cosine = 6;

    /// von Mises-Fisher (vMF) lobe concentration on each seed direction
    /// (angular width ~ 1/sqrt(kappa)).
    double vmf_kappa = 128.0;
};

///
/// Options for occluded-facet visibility sampling.
///
struct OccludedFacetSamplerOptions
{
    /// Adaptive multiple importance sampling (MIS) with a cosine + seed-guided mixture is enabled
    /// by default. Set to `std::nullopt` for plain cosine-weighted hemisphere sampling.
    std::optional<AdaptiveOptions> adaptive = AdaptiveOptions{};

    /// How much a facet size counts toward keeping it. `0` = size-independent, `1` = area-weighted.
    double size_influence = 0.5;

    /// Early-keep sampling retirement cutoff on the size-weighted visibility measure
    /// `M = mean_visibility * (area_f / mean_face_area)^size_influence`.
    /// Also bounds the subsequent filter's visibility_threshold, not the facet's measure.
    double max_visibility_threshold = 5e-06;

    /// Per-facet confidence for anytime-valid early-keep retirement. Under the betting-test
    /// assumptions, the chance of incorrectly retiring a facet is at most `1 - confidence` over
    /// any number of batches. Higher values retire later. This does not control final removals or
    /// joint confidence across the scene.
    double confidence = 0.9;

    /// Total ray budget. If 0, a non-null cancellation flag is required.
    uint64_t num_rays = 1600000000ULL;

    /// Rays per batch. Cancellation and progress are checked at batch boundaries.
    uint64_t batch_size = 50000000ULL;
};

///
/// How a facet's measure is reconciled across the instances of its source mesh.
///
enum class InstancingPolicy {
    /// One output mesh per input instance; instancing is lost.
    FlattenInstances,
    /// Preserve instancing; measure is the max (most visible) over its instances.
    Max,
    /// Preserve instancing; measure is the mean over its instances.
    Average,
};

/// Options for applying cached @ref OccludedFacetsFilter estimates to a new result.
struct OccludedFacetFilterOptions
{
    /// Finite cutoff satisfying `0 <= visibility_threshold <= sampler.max_visibility_threshold`.
    /// Without smoothing, keep iff the size-weighted visibility measure is >= visibility_threshold.
    /// Positive smoothing may override these independent labels.
    double visibility_threshold = OccludedFacetSamplerOptions{}.max_visibility_threshold;

    /// Optional finite non-negative smoothness. `std::nullopt` disables graph cutting; 0 gives
    /// independent thresholding. Positive values favor coherent keep/cull regions.
    std::optional<double> graphcut_smoothness = std::nullopt;

    /// Aggregate instance measures before classification/export; Max/Average preserve instancing.
    InstancingPolicy instancing = InstancingPolicy::Max;

    /// Optional Scalar facet visibility attribute, exported before any pruning.
    std::optional<std::string> visibility_attribute_name = std::nullopt;

    /// Optional uint8 facet keep mask (1 keep, 0 cull). When specified, no facet is removed.
    std::optional<std::string> keep_attribute_name = std::nullopt;
};

/// Options for filtering cached facet estimates with a user-supplied view-error metric.
struct OccludedFacetViewErrorOptions
{
    /// Finite non-negative error budget that every evaluated view must satisfy.
    double max_view_error = 0.01;

    /// Finite positive graph-cut weights, sorted and deduplicated before the sweep.
    /// Empty selects an automatic sweep scaled by the largest cached measure.
    std::vector<double> graphcut_smoothness_candidates = {};

    /// Positive number of binary-search iterations per primary threshold search.
    uint64_t num_threshold_iterations = 20;

    /// Positive number of iterations per refinement search after full-view verification fails.
    uint64_t num_refinement_iterations = 10;

    /// Positive number of errors the evaluator writes when the view-index list is empty.
    uint64_t num_views = 1;

    /// Fraction of baseline removal that may be sacrificed for smoother keep/cull regions.
    /// nullopt disables the sweep; 0 preserves removal count, 1 permits sacrificing all removal.
    std::optional<double> smoothness_tradeoff = 0.5;
};

/// Chosen parameters and statistics for the flattened output of view-error filtering.
struct OccludedFacetViewErrorResult
{
    /// Chosen graph-cut weight; 0 for threshold-only filtering or an all-kept fallback.
    double graphcut_smoothness = 0.0;

    /// Chosen score cutoff, not constrained by the sampler's retirement cutoff.
    /// All-kept fallbacks use threshold 0 and graphcut_smoothness 0.
    double threshold = 0.0;

    /// Fraction of input instance-facets removed, not weighted by area.
    double removed_fraction = 0.0;

    /// Kept facets with no kept edge-neighbor, not the number of disconnected components.
    uint64_t num_isolated_facets = 0;

    /// Achieved worst error across all views for the returned mask, including failed baselines.
    double worst_view_error = std::numeric_limits<double>::infinity();

    /// Whether the returned mask satisfies the view-error budget; not an optimality guarantee.
    bool within_error_budget = false;
};

/// Given a flattened uint8 keep mask (1 = keep) and requested camera indices, write each view's
/// error against the original scene. Empty indices request all options.num_views errors.
/// Mask order is source mesh, instance, then local facet. Calls are synchronous and repeated;
/// keep source geometry and reference images available without rebuilding the scene per probe.
using RenderErrorEvaluator = function_ref<
    void(span<const uint8_t> keep_mask, span<const uint64_t> view_indices, span<double> errors)>;

///
/// Sample facet visibility once and reuse the cached estimates for measurement or filtering.
///
/// The scene must outlive this filter and remain unchanged while the filter is in use, as the
/// cached measures depend on its geometry, topology, transforms, and occluders.
///
/// Thresholds applied through apply() must satisfy `0 <= visibility_threshold <=
/// sampler_options.max_visibility_threshold`.
///
/// This class is move-only. Const member functions may be called concurrently.
///
/// @tparam     Scalar  Mesh scalar type.
/// @tparam     Index   Mesh index type.
///
template <typename Scalar, typename Index>
class LA_RAYCASTING_API OccludedFacetsFilter
{
public:
    ///
    /// Estimate per-facet visibility by sampling rays, then cache the results for later filtering.
    /// The source scene must remain alive and unchanged while this object is in use.
    ///
    /// @param[in]     scene            Source triangle-mesh scene, kept alive and unchanged through
    ///                                 all filter uses.
    /// @param[in]     sampler_options  Sampling options, ray budget, batch size, and maximum
    ///                                 filtering threshold.
    /// @param[in,out] progress         Sampling progress callback.
    /// @param[in]     is_occluder      Whether `(mesh_index, instance_index)` should block rays.
    /// @param[in]     cancel           Optional sampling cancellation flag; retains partial
    ///                                 estimates.
    ///
    OccludedFacetsFilter(
        const scene::SimpleScene<Scalar, Index, 3>& scene,
        const OccludedFacetSamplerOptions& sampler_options,
        ProgressCallback& progress,
        function_ref<bool(Index mesh_index, Index instance_index)> is_occluder =
            [](Index, Index) { return true; },
        const std::atomic_bool* cancel = nullptr);

    ~OccludedFacetsFilter();
    OccludedFacetsFilter(const OccludedFacetsFilter&) = delete;
    OccludedFacetsFilter(OccludedFacetsFilter&&) noexcept;
    OccludedFacetsFilter& operator=(const OccludedFacetsFilter&) = delete;
    OccludedFacetsFilter& operator=(OccludedFacetsFilter&&) noexcept;

    ///
    /// Export facet visibility estimate as a per-facet Scalar attribute.
    /// Measures are aggregated across instances according to the requested policy.
    ///
    /// @param[in]  attribute_name  Name of the per-facet Scalar attribute to create on each output mesh.
    /// @param[in]  policy          The instancing policy to use when aggregating measures across instances.
    ///
    /// @return     A new scene with the measured occluded facets as a per-facet Scalar attribute.
    ///
    scene::SimpleScene<Scalar, Index, 3> estimate_visibility(
        std::string_view attribute_name,
        InstancingPolicy policy = InstancingPolicy::Max) const;

    ///
    /// Apply occluded facet filter to the scene, removing facets below the requested visibility
    /// threshold. The filter uses the cached constructor estimates without resampling, so it should
    /// be fast. Visibility measures are aggregated across instances according to the requested
    /// policy. If smoothing is requested, a graph-cut is applied to the facet graph before pruning.
    /// If visibility_attribute_name is present, the per-facet visibility measures are exported as a
    /// Scalar attribute before any pruning. If keep_attribute_name is present, no facets are
    /// removed; instead, a uint8 facet mask (1 keep, 0 cull) is exported.
    ///
    /// @param[in]  options  Classification, instancing, and optional attribute-export options.
    ///
    /// @return     A new scene, pruned unless keep_attribute_name is present.
    ///
    /// @throws     Error  For invalid thresholds/weights, identical output names, or attribute creation
    ///                    errors (including existing source attributes; no overwriting).
    ///
    scene::SimpleScene<Scalar, Index, 3> apply(const OccludedFacetFilterOptions& options) const;

    /// Search graph-cut parameters using cached measures and a user-supplied view-error budget.
    /// Instances are flattened. Scores guide the heuristic search; the evaluator validates each
    /// candidate, so chosen thresholds may exceed the sampling retirement cutoff. The output is
    /// assembled from the same global graph used during probing, without resampling.
    ///
    /// @param[in] evaluate  Synchronous per-view error evaluator in mesh-instance-facet order.
    /// @param[in] options   Error budget and search controls.
    /// @return A new flattened scene and its chosen parameters, statistics, and feasibility.
    /// @throws Error For invalid options, invalid evaluator errors, or a moved-from filter.
    std::pair<scene::SimpleScene<Scalar, Index, 3>, OccludedFacetViewErrorResult>
    apply_with_view_error(
        RenderErrorEvaluator evaluate,
        const OccludedFacetViewErrorOptions& options) const;

    ///
    /// Maximum allowed apply() visibility_threshold parameter: the sampling retirement cutoff.
    ///
    /// @return     The sampler's max_visibility_threshold, not a bound on facet visibility.
    ///
    /// @throws     Error  if this object has been moved from.
    ///
    double get_max_visibility_threshold() const;

private:
    struct Impl;
    value_ptr<Impl> m_impl;
};

///
/// Options for remove_occluded_facets().
///
struct RemoveOccludedFacetsOptions
{
    /// Options controlling occluded-facet visibility sampling.
    OccludedFacetSamplerOptions sampler_options = {};

    /// How to reconcile a facet's measure across the instances of its source mesh.
    InstancingPolicy instancing = InstancingPolicy::Max;

    /// Optional non-negative finite graph-cut smoothness weight. `std::nullopt` disables graph
    /// cutting; 0 gives independent thresholding, while positive values favor coherent regions.
    /// Applied per output mesh, after @ref instancing.
    std::optional<double> graphcut_smoothness = std::nullopt;
};

///
/// Estimate each facet's visibility measure and write it to a named per-facet Scalar attribute.
///
/// Instances are reconciled per @p options.instancing (see @ref InstancingPolicy). The sampler runs
/// at `options.sampler_options.max_visibility_threshold`, so a caller can re-threshold the measures
/// without re-tracing using `0 <= visibility_threshold <= max_visibility_threshold`. This bounds
/// the filtering cutoff, not the measures themselves.
///
/// @param[in]      scene           Input scene. Every referenced mesh must be a triangle mesh.
/// @param[in]      attribute_name  Name of the per-facet Scalar attribute to create on each output mesh.
/// @param[in]      options         Options.
/// @param[in,out]  progress        Progress callback, updated at batch boundaries.
/// @param[in]      is_occluder     Whether `(mesh_index, instance_index)` should block rays.
///                                Non-occluders are still tested for visibility.
/// @param[in]      cancel          Optional cancellation flag.
///
/// @return     The scene with per-facet visibility measures.
///
/// @tparam     Scalar    Mesh scalar type.
/// @tparam     Index     Mesh index type.
///
template <typename Scalar, typename Index>
LA_RAYCASTING_API scene::SimpleScene<Scalar, Index, 3> estimate_facet_visibility(
    const scene::SimpleScene<Scalar, Index, 3>& scene,
    std::string_view attribute_name,
    const RemoveOccludedFacetsOptions& options,
    ProgressCallback& progress,
    function_ref<bool(Index mesh_index, Index instance_index)> is_occluder =
        [](Index, Index) { return true; },
    const std::atomic_bool* cancel = nullptr);

///
/// Build a new scene with facets not visible from the outside removed.
///
/// Instances are reconciled per @p options.instancing (see @ref InstancingPolicy).
///
/// @param[in]      scene        Input scene.
/// @param[in]      options      Options.
/// @param[in,out]  progress     Progress callback, updated at batch boundaries.
/// @param[in]      is_occluder  Whether `(mesh_index, instance_index)` should block rays.
///                             Non-occluders are still tested for visibility.
/// @param[in]      cancel       Optional cancellation flag.
///
/// @return     The filtered scene.
///
/// @tparam     Scalar    Mesh scalar type.
/// @tparam     Index     Mesh index type.
///
template <typename Scalar, typename Index>
LA_RAYCASTING_API scene::SimpleScene<Scalar, Index, 3> remove_occluded_facets(
    const scene::SimpleScene<Scalar, Index, 3>& scene,
    const RemoveOccludedFacetsOptions& options,
    ProgressCallback& progress,
    function_ref<bool(Index mesh_index, Index instance_index)> is_occluder =
        [](Index, Index) { return true; },
    const std::atomic_bool* cancel = nullptr);

/// Sample visibility once and filter facets using a user-supplied view-error budget.
/// This convenience wrapper constructs an OccludedFacetsFilter and calls apply_with_view_error().
/// Instances are flattened; options.instancing and options.graphcut_smoothness are ignored.
///
/// @param[in]      scene           Input scene.
/// @param[in]      evaluate        Render-error oracle. @see RenderErrorEvaluator.
/// @param[in]      options         Visibility sampler options; instancing and smoothness are ignored.
/// @param[in]      solver_options  View-error budget and search controls.
/// @param[in,out]  progress        Progress callback for visibility estimation.
/// @param[in]      is_occluder     Whether `(mesh_index, instance_index)` should block rays.
///                                Non-occluders are still tested for visibility.
/// @param[in]      cancel          Optional cancellation flag.
///
/// @return The filtered scene and the solved graph-cut parameters.
///
/// @tparam Scalar  Mesh scalar type.
/// @tparam Index   Mesh index type.
///
template <typename Scalar, typename Index>
LA_RAYCASTING_API std::pair<scene::SimpleScene<Scalar, Index, 3>, OccludedFacetViewErrorResult>
remove_occluded_facets_with_view_error(
    const scene::SimpleScene<Scalar, Index, 3>& scene,
    RenderErrorEvaluator evaluate,
    const RemoveOccludedFacetsOptions& options,
    const OccludedFacetViewErrorOptions& solver_options,
    ProgressCallback& progress,
    function_ref<bool(Index mesh_index, Index instance_index)> is_occluder =
        [](Index, Index) { return true; },
    const std::atomic_bool* cancel = nullptr);

/// @}

} // namespace lagrange::raycasting
