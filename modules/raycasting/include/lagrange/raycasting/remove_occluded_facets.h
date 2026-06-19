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

namespace lagrange::raycasting {

///
/// @addtogroup module-raycasting
/// @{
///

///
/// Options for OccludedFacetSampler.
///
struct OccludedFacetSamplerOptions
{
    /// Standard deviation of the Gaussian jitter applied to seed directions in adaptive batches.
    double jitter_sigma = 0.025;

    /// Number of independent escapes a facet must accumulate before being marked visible. K=1
    /// is the original "first escape wins" behavior; K=2-3 dampens single-ray noise (a lucky
    /// ray sneaking through a hairline gap no longer flips a facet on its own). Counts
    /// saturate at 255.
    uint8_t visibility_threshold = 3;
};

///
/// Stateful algorithm for finding occluded facets across every instance of a scene. Each
/// instance is gated independently with its own world-space facets.
///
/// Combines two sampling strategies that can be alternated progressively:
///  - @ref run_normal_batch : cosine-weighted hemisphere sampling. The first escape direction
///    is cached against the facet that escaped.
///  - @ref run_adaptive_batch : for each still-occluded facet, casts jittered rays along
///    cached escape directions of its edge-adjacent visible neighbors. Exploits topological
///    coherence — an escape from a neighbor often also escapes from here.
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

    /// Cosine-weighted hemisphere batch. Caches each facet's first-discovered escape direction
    /// for later adaptive batches.
    void run_normal_batch(uint64_t num_rays);

    /// Adaptive batch using cached escape directions of 1-ring visible neighbors. Skips facets
    /// with no visible neighbor.
    void run_adaptive_batch(uint64_t num_rays);

    /// Cosine-weighted hemisphere batch with no escape caching — baseline for benchmarking
    /// the adaptive mode against pure sampling.
    void run_brute_force_batch(uint64_t num_rays);

    /// Whether the facet has been marked visible so far.
    bool is_visible(uint64_t global_facet_index) const;

    /// Rays cast so far for the facet.
    uint64_t num_rays_cast(uint64_t global_facet_index) const;

    /// Total number of facets across all instances. Multi-instance meshes are counted once
    /// per instance.
    uint64_t num_facets() const;

    /// Per-instance metadata for mapping global facet indices to (mesh, instance, local facet).
    span<const InstanceInfo> instances() const;

private:
    struct Impl;
    value_ptr<Impl> m_impl;
};

///
/// Loop options for @ref estimate_occluded_facets() and @ref remove_occluded_facets().
///
struct OccludedFacetEstimateOptions
{
    /// Total ray budget. If 0, the loop runs until cancelled or converged — at least one of
    /// `num_rays>0`, @ref until_converged, or a non-null cancel flag is required.
    uint64_t num_rays = 1600000000ULL;

    /// Rays per batch. Cancellation, progress, and convergence are checked at cycle boundaries
    /// (one batch per cycle in brute-force, one normal + @ref num_adaptive_per_normal adaptive
    /// otherwise).
    uint64_t batch_size = 20000000ULL;

    /// Adaptive batches per normal batch. 0 reduces to pure cosine sampling. Ignored when
    /// @ref brute_force is true.
    uint64_t num_adaptive_per_normal = 6;

    /// Run brute-force batches only — baseline for benchmarking against the adaptive mode.
    bool brute_force = false;

    /// Stop early when a cycle finds no new visible facets.
    bool until_converged = false;
};

///
/// Drive an @ref OccludedFacetSampler progressively until the budget is exhausted, the search
/// converges, or cancellation is requested. Logs per-cycle progress via @c lagrange::logger()
/// and reports a normalized [0, 1] fraction to @p progress.
///
/// The caller can inspect @p sampler after the call returns to retrieve per-element stats,
/// build an output scene, etc.
///
/// @param[in,out]  sampler   Sampler to drive.
/// @param[in]      options   Estimate options.
/// @param[in,out]  progress  Progress callback (default-constructed = silent).
/// @param[in]      cancel    Optional cancellation flag, polled at cycle boundaries.
///
/// @tparam         Scalar    Mesh scalar type.
/// @tparam         Index     Mesh index type.
///
template <typename Scalar, typename Index>
LA_RAYCASTING_API void estimate_occluded_facets(
    OccludedFacetSampler<Scalar, Index>& sampler,
    const OccludedFacetEstimateOptions& options,
    ProgressCallback& progress,
    const std::atomic_bool* cancel = nullptr);

///
/// Options for remove_occluded_facets().
///
struct RemoveOccludedFacetsOptions
{
    /// Estimate-loop options forwarded to @ref estimate_occluded_facets().
    OccludedFacetEstimateOptions estimate_options = {};

    /// Options forwarded to the underlying @ref OccludedFacetSampler.
    OccludedFacetSamplerOptions sampler_options = {};
};

///
/// Build a new scene with facets not visible from the outside removed.
///
/// The output contains one unique mesh per input instance: instances of the same source mesh
/// can end up with different facets culled, so the input's instancing cannot be preserved.
///
/// @param[in]      scene        Input scene.
/// @param[in]      options      Options.
/// @param[in,out]  progress     Progress callback (see @ref estimate_occluded_facets).
/// @param[in]      is_occluder  Forwarded to the underlying @ref OccludedFacetSampler ctor.
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
