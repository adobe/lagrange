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
#include <optional>
#include <string_view>

namespace lagrange::raycasting {

///
/// @addtogroup module-raycasting
/// @{
///

///
/// Options for adaptive mode of OccludedFacetSampler.
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
/// Options for OccludedFacetSampler.
///
struct OccludedFacetSamplerOptions
{
    /// Adaptive multiple importance sampling (MIS) with a cosine + seed-guided mixture is enabled
    /// by default. Set to `std::nullopt` for plain cosine-weighted hemisphere sampling.
    std::optional<AdaptiveOptions> adaptive = AdaptiveOptions{};

    /// How much a facet size counts toward keeping it. `0` = size-independent, `1` = area-weighted.
    double size_influence = 0.5;

    /// Keep-threshold for
    /// `M = mean_visibility * (area_f / mean_face_area)^size_influence`.
    /// A facet is kept when `M >= threshold`.
    double threshold = 5e-06;

    /// Per-facet confidence for anytime-valid early-keep retirement. Under the betting-test
    /// assumptions, the chance of incorrectly retiring a facet is at most `1 - confidence` over
    /// any number of batches. Higher values retire later. This does not control final removals or
    /// joint confidence across the scene.
    double confidence = 0.9;
};

namespace internal {

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
    OccludedFacetSampler(OccludedFacetSampler&&) noexcept;
    OccludedFacetSampler& operator=(OccludedFacetSampler&&) noexcept;

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
    struct Impl;
    value_ptr<Impl> m_impl;
};

} // namespace internal

///
/// Ray-budget options for @ref estimate_occluded_facet_measures() and
/// @ref remove_occluded_facets().
///
struct OccludedFacetEstimateOptions
{
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

///
/// Options for remove_occluded_facets().
///
struct RemoveOccludedFacetsOptions
{
    /// Options forwarded to the underlying @ref internal::OccludedFacetSampler.
    OccludedFacetSamplerOptions sampler_options = {};

    /// Ray-budget and batch-size options.
    OccludedFacetEstimateOptions estimate_options = {};

    /// How to reconcile a facet's measure across the instances of its source mesh.
    InstancingPolicy instancing = InstancingPolicy::Max;
};

///
/// Estimate each facet's visibility measure and write it to a named per-facet Scalar attribute.
///
/// Instances are reconciled per @p options.instancing (see @ref InstancingPolicy). The sampler runs
/// at `options.sampler_options.threshold`, so a caller can re-threshold the measures without
/// re-tracing.
///
/// @param[in]      scene           Input scene. Every referenced mesh must be a triangle mesh.
/// @param[in]      attribute_name  Name of the per-facet Scalar attribute to create on each output mesh.
/// @param[in]      options         Options.
/// @param[in,out]  progress        Progress callback, updated at batch boundaries.
/// @param[in]      is_occluder     Forwarded to the underlying @ref internal::OccludedFacetSampler ctor.
/// @param[in]      cancel          Optional cancellation flag.
///
/// @return     The scene with per-facet visibility measures.
///
/// @tparam     Scalar    Mesh scalar type.
/// @tparam     Index     Mesh index type.
///
template <typename Scalar, typename Index>
LA_RAYCASTING_API scene::SimpleScene<Scalar, Index, 3> estimate_occluded_facet_measures(
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
/// @param[in]      is_occluder  Forwarded to the underlying @ref internal::OccludedFacetSampler ctor.
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

/// @}

} // namespace lagrange::raycasting
