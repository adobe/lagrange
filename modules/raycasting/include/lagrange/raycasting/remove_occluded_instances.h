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
#include <lagrange/utils/value_ptr.h>

#include <atomic>
#include <utility>
#include <vector>

namespace lagrange::raycasting {

///
/// @addtogroup module-raycasting
/// @{
///

///
/// Options for OccludedInstanceSampler.
///
struct OccludedInstanceSamplerOptions
{
    /// Keep-threshold for `M = mean_visibility * (area / R)^size_influence`, where
    /// `mean_visibility` is the cosine-weighted escaped fraction and R is the scene AABB area.
    double threshold = 1e-06;

    /// Exponent on `area / R` in the measure above. `0` = size-independent visibility;
    /// `1` = area-weighted visibility.
    double size_influence = 0.5;

    /// Per-instance confidence used by each batch-end early-keep test. Higher values require more
    /// evidence before sampling stops. This does not control final removals or joint confidence
    /// across the scene.
    double confidence = 0.9;
};

namespace internal {

///
/// Stateful algorithm for finding occluded instances in a scene. Each call to @ref run_batch
/// distributes rays over instances not yet decided, progressively improving the result. Per-instance
/// results are addressed by `(mesh_index, instance_index)`.
///
/// @note Only 3D scenes are supported.
///
/// @tparam Scalar  Mesh scalar type.
/// @tparam Index   Mesh index type.
///
template <typename Scalar, typename Index>
class LA_RAYCASTING_API OccludedInstanceSampler
{
public:
    ///
    /// Build the ray caster and per-instance precomputation.
    ///
    /// @param[in]  scene         Scene to process.
    /// @param[in]  options       Sampler options (keep-threshold, size influence, confidence).
    /// @param[in]  is_occluder   Returns whether `(mesh_index, instance_index)` should block
    ///                           rays. Non-occluder instances are still tested for visibility
    ///                           but do not contribute to the ray-caster scene.
    ///
    explicit OccludedInstanceSampler(
        const scene::SimpleScene<Scalar, Index, 3>& scene,
        const OccludedInstanceSamplerOptions& options = {},
        function_ref<bool(Index mesh_index, Index instance_index)> is_occluder = [](Index, Index) {
            return true;
        });

    ~OccludedInstanceSampler();
    OccludedInstanceSampler(OccludedInstanceSampler&&) noexcept;
    OccludedInstanceSampler& operator=(OccludedInstanceSampler&&) noexcept;

    /// Run a batch distributing @p num_rays across instances not yet decided.
    void run_batch(uint64_t num_rays);

    /// Whether the instance is currently kept: `visibility_measure(mesh, instance) >= threshold`.
    [[nodiscard]] bool is_visible(Index mesh_index, Index instance_index) const;

    /// The instance's visibility measure
    /// `mean_visibility * (area / R)^size_influence`, where `mean_visibility` is the estimated
    /// cosine-weighted escaped fraction in [0, 1].
    [[nodiscard]] double visibility_measure(Index mesh_index, Index instance_index) const;

    /// Rays cast so far for the instance.
    [[nodiscard]] uint64_t num_rays_cast(Index mesh_index, Index instance_index) const;

    /// Total number of instances across all meshes.
    [[nodiscard]] Index num_instances() const;

    /// Number of instances confidently above the keep threshold and retired from sampling. This
    /// is useful to progressive callers for reporting and detecting that all instances retired.
    [[nodiscard]] Index num_retired() const;

    /// Aggregate `(number of currently-visible instances, total rays cast)` for progress reporting
    /// without rescanning the scene's `(mesh_index, instance_index)` address space.
    [[nodiscard]] std::pair<Index, uint64_t> progress() const;

private:
    struct Impl;
    value_ptr<Impl> m_impl;
};

} // namespace internal

///
/// Loop options for @ref estimate_occluded_instances() and @ref remove_occluded_instances().
///
struct OccludedInstanceEstimateOptions
{
    /// Total ray budget. If 0, the loop runs until cancelled or convergence is requested — at
    /// least one of `num_rays>0`, @ref until_converged, or a non-null cancel flag is required.
    uint64_t num_rays = 1600000000ULL;

    /// Rays per batch. Cancellation, progress, and convergence are checked at batch boundaries.
    uint64_t batch_size = 50000000ULL;

    /// After the initial batch, stop when a batch confidently retires no new instances.
    /// This is a batch-size-dependent heuristic; use @ref num_rays for fixed-budget estimates.
    bool until_converged = false;
};

///
/// Options for remove_occluded_instances() and estimate_occluded_instance_measures().
///
struct RemoveOccludedInstancesOptions
{
    /// Sampler options (keep-threshold, size influence, confidence).
    OccludedInstanceSamplerOptions sampler_options = {};

    /// Estimate-loop options.
    OccludedInstanceEstimateOptions estimate_options = {};
};

///
/// Estimate the per-instance visibility measure for a scene. The sampler runs at
/// `options.sampler_options.threshold`. The returned measures let a caller keep instances at any
/// smaller threshold without re-tracing: keep `(mesh_index, instance_index)` iff
/// `measures[mesh_index][instance_index] >= threshold`.
///
/// @note Only 3D scenes are supported.
///
/// @param[in]      scene        Scene to process.
/// @param[in]      options      Options.
/// @param[in,out]  progress     Progress callback.
/// @param[in]      is_occluder  Forwarded to @ref internal::OccludedInstanceSampler ctor.
/// @param[in]      cancel       Optional cancellation flag.
///
/// @return     Per-instance measures indexed `[mesh_index][instance_index]`.
///
/// @tparam     Scalar    Mesh scalar type.
/// @tparam     Index     Mesh index type.
///
template <typename Scalar, typename Index>
LA_RAYCASTING_API std::vector<std::vector<double>> estimate_occluded_instance_measures(
    const scene::SimpleScene<Scalar, Index, 3>& scene,
    const RemoveOccludedInstancesOptions& options,
    ProgressCallback& progress,
    function_ref<bool(Index mesh_index, Index instance_index)> is_occluder =
        [](Index, Index) { return true; },
    const std::atomic_bool* cancel = nullptr);

///
/// Remove instances whose visibility measure is below `options.sampler_options.threshold`.
/// Convenience wrapper around @ref internal::OccludedInstanceSampler and
/// `lagrange::scene::filter_instances`.
///
/// @note Only 3D scenes are supported.
///
/// @param[in]      scene        Scene to process.
/// @param[in]      options      Options.
/// @param[in,out]  progress     Progress callback.
/// @param[in]      is_occluder  Forwarded to the underlying sampler.
/// @param[in]      cancel       Optional cancellation flag.
///
/// @tparam     Scalar    Mesh scalar type.
/// @tparam     Index     Mesh index type.
///
template <typename Scalar, typename Index>
LA_RAYCASTING_API scene::SimpleScene<Scalar, Index, 3> remove_occluded_instances(
    const scene::SimpleScene<Scalar, Index, 3>& scene,
    const RemoveOccludedInstancesOptions& options,
    ProgressCallback& progress,
    function_ref<bool(Index mesh_index, Index instance_index)> is_occluder =
        [](Index, Index) { return true; },
    const std::atomic_bool* cancel = nullptr);

///
/// @deprecated Use @ref estimate_occluded_instance_measures. This overload reports only the
/// occluded instances via @p callback. It uses `std::numeric_limits<double>::min()` as the
/// keep-threshold to preserve the original first-observed-escape behavior.
///
template <typename Scalar, typename Index>
[[deprecated("Use estimate_occluded_instance_measures().")]]
LA_RAYCASTING_API void estimate_occluded_instances(
    const scene::SimpleScene<Scalar, Index, 3>& scene,
    function_ref<void(Index mesh_index, Index instance_index)> callback,
    const OccludedInstanceEstimateOptions& options,
    ProgressCallback& progress,
    function_ref<bool(Index mesh_index, Index instance_index)> is_occluder =
        [](Index, Index) { return true; },
    const std::atomic_bool* cancel = nullptr);

///
/// @deprecated Use the @ref RemoveOccludedInstancesOptions overload of
/// @ref remove_occluded_instances, which exposes the keep-threshold. This overload uses
/// `std::numeric_limits<double>::min()` as the keep-threshold to preserve the original
/// first-observed-escape behavior.
///
template <typename Scalar, typename Index>
[[deprecated("Use the remove_occluded_instances overload taking RemoveOccludedInstancesOptions.")]]
LA_RAYCASTING_API scene::SimpleScene<Scalar, Index, 3> remove_occluded_instances(
    const scene::SimpleScene<Scalar, Index, 3>& scene,
    const OccludedInstanceEstimateOptions& options,
    ProgressCallback& progress,
    function_ref<bool(Index mesh_index, Index instance_index)> is_occluder =
        [](Index, Index) { return true; },
    const std::atomic_bool* cancel = nullptr);

/// @}

} // namespace lagrange::raycasting
