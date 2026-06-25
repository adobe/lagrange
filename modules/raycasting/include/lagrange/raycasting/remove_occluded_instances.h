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

namespace lagrange::raycasting {

///
/// @addtogroup module-raycasting
/// @{
///

///
/// Stateful algorithm for finding occluded instances in a scene. Each call to @ref run_batch
/// distributes rays over instances not yet marked visible, progressively improving the result.
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
    /// @param[in]  is_occluder   Returns whether `(mesh_index, instance_index)` should block
    ///                           rays. Non-occluder instances are still tested for visibility
    ///                           but do not contribute to the ray-caster scene.
    ///
    explicit OccludedInstanceSampler(
        const scene::SimpleScene<Scalar, Index, 3>& scene,
        function_ref<bool(Index mesh_index, Index instance_index)> is_occluder = [](Index, Index) {
            return true;
        });

    ~OccludedInstanceSampler();
    OccludedInstanceSampler(OccludedInstanceSampler&&) noexcept;
    OccludedInstanceSampler& operator=(OccludedInstanceSampler&&) noexcept;

    /// Run a batch distributing @p num_rays across instances not yet marked visible.
    void run_batch(uint64_t num_rays);

    /// Whether the instance has been marked visible so far.
    bool is_visible(Index global_index) const;

    /// Rays cast so far for the instance.
    uint64_t num_rays_cast(Index global_index) const;

    /// Total number of instances.
    Index num_instances() const;

private:
    struct Impl;
    value_ptr<Impl> m_impl;
};

///
/// Loop options for @ref estimate_occluded_instances() and @ref remove_occluded_instances().
///
struct OccludedInstanceEstimateOptions
{
    /// Total ray budget. If 0, the loop runs until cancelled or converged — at least one of
    /// `num_rays>0`, @ref until_converged, or a non-null cancel flag is required.
    uint64_t num_rays = 1600000000ULL;

    /// Rays per batch. Cancellation, progress, and convergence are checked at batch boundaries.
    uint64_t batch_size = 20000000ULL;

    /// Stop early when a batch finds no new visible instances.
    bool until_converged = false;
};

///
/// Drive an @ref OccludedInstanceSampler progressively. See @ref estimate_occluded_facets for the
/// shared semantics; the only difference here is per-batch (rather than per-cycle) granularity.
///
/// @param[in,out]  sampler   Sampler to drive.
/// @param[in]      options   Estimate options.
/// @param[in,out]  progress  Progress callback (default-constructed = silent).
/// @param[in]      cancel    Optional cancellation flag, polled at batch boundaries.
///
/// @tparam         Scalar    Mesh scalar type.
/// @tparam         Index     Mesh index type.
///
template <typename Scalar, typename Index>
LA_RAYCASTING_API void estimate_occluded_instances(
    OccludedInstanceSampler<Scalar, Index>& sampler,
    const OccludedInstanceEstimateOptions& options,
    ProgressCallback& progress,
    const std::atomic_bool* cancel = nullptr);

///
/// @overload
///
/// Convenience wrapper that builds an @ref OccludedInstanceSampler internally and reports each
/// occluded instance via @p callback.
///
/// @note Only 3D scenes are supported.
///
/// @param[in]      scene        Scene to process.
/// @param[in]      callback     Called as `callback(mesh_index, instance_index)`.
/// @param[in]      options      Options.
/// @param[in,out]  progress     Progress callback.
/// @param[in]      is_occluder  Forwarded to @ref OccludedInstanceSampler ctor: returns
///                              whether `(mesh_index, instance_index)` should block rays.
///                              Non-occluder instances are still tested for visibility but
///                              don't contribute to the ray-caster scene.
/// @param[in]      cancel       Optional cancellation flag.
///
/// @tparam     Scalar    Mesh scalar type.
/// @tparam     Index     Mesh index type.
///
template <typename Scalar, typename Index>
LA_RAYCASTING_API void estimate_occluded_instances(
    const scene::SimpleScene<Scalar, Index, 3>& scene,
    function_ref<void(Index mesh_index, Index instance_index)> callback,
    const OccludedInstanceEstimateOptions& options,
    ProgressCallback& progress,
    function_ref<bool(Index mesh_index, Index instance_index)> is_occluder =
        [](Index, Index) { return true; },
    const std::atomic_bool* cancel = nullptr);

///
/// Remove fully-occluded mesh instances. Convenience wrapper around
/// @ref estimate_occluded_instances + `lagrange::scene::filter_instances`.
///
/// @note Only 3D scenes are supported.
///
/// @param[in]      scene        Scene to process.
/// @param[in]      options      Options.
/// @param[in,out]  progress     Progress callback.
/// @param[in]      is_occluder  Forwarded to the underlying sampler — see
///                              @ref estimate_occluded_instances. Non-occluders are still
///                              candidates for removal but don't block rays from others.
/// @param[in]      cancel       Optional cancellation flag.
///
/// @tparam     Scalar    Mesh scalar type.
/// @tparam     Index     Mesh index type.
///
template <typename Scalar, typename Index>
LA_RAYCASTING_API scene::SimpleScene<Scalar, Index, 3> remove_occluded_instances(
    const scene::SimpleScene<Scalar, Index, 3>& scene,
    const OccludedInstanceEstimateOptions& options,
    ProgressCallback& progress,
    function_ref<bool(Index mesh_index, Index instance_index)> is_occluder =
        [](Index, Index) { return true; },
    const std::atomic_bool* cancel = nullptr);

/// @}

} // namespace lagrange::raycasting
