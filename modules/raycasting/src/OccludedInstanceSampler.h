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
#include <lagrange/raycasting/remove_occluded_instances.h>
#include <lagrange/scene/SimpleScene.h>
#include <lagrange/utils/function_ref.h>

#include <cstdint>
#include <utility>
#include <vector>

namespace lagrange::raycasting {

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
    OccludedInstanceSampler(const OccludedInstanceSampler&) = delete;
    OccludedInstanceSampler(OccludedInstanceSampler&&) noexcept = default;
    OccludedInstanceSampler& operator=(const OccludedInstanceSampler&) = delete;
    OccludedInstanceSampler& operator=(OccludedInstanceSampler&&) noexcept = default;

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
    RayCaster m_ray_caster;

    std::vector<detail::InstanceData<Scalar>> m_instances;

    /// Inter-instance ray-allocation weight ∝ 1/tau_i = (area / R)^size_influence. Rays track each
    /// instance's statistical difficulty: low-tau (large) instances need the most to resolve.
    std::vector<Scalar> m_instance_weights;

    /// Cached `(area / R)^size_influence`, used by both the public measure and ray allocation.
    std::vector<double> m_instance_size_factor;

    /// Per-instance escape-fraction threshold `tau = threshold / size_factor`.
    std::vector<double> m_instance_tau;

    /// Public keep threshold applied to visibility_measure().
    double m_threshold = 0.0;

    /// Retire-KEEP flag: set once confidently above tau (sampling then stops). Occluded instances
    /// never retire; they sample to the ray budget.
    std::vector<uint8_t> m_retired;
    std::vector<uint64_t> m_num_escaped_rays;
    std::vector<uint64_t> m_num_rays_cast;
    double m_alpha = 0.01;
    /// Prefix sum over meshes: flat instance index = m_mesh_offset[mesh] + instance.
    std::vector<Index> m_mesh_offset;
    /// Reused per-instance ray-allocation scratch; process_instance() is called sequentially.
    std::vector<uint64_t> m_ray_boundaries;

    Index flat(Index mesh_index, Index instance_index) const;

    Scalar instance_active_weight(size_t i) const;

    /// Estimated cosine-weighted escaped fraction of the instance, in [0, 1]; 0 before sampling.
    double mean_visibility(size_t i) const;

    /// Size-weighted visibility measure: mean_visibility * (area / R)^size_influence.
    double visibility_measure(size_t i) const;

    /// Point-estimate keep decision on the public visibility measure.
    bool kept(size_t i) const;

    void process_instance(size_t i, uint64_t instance_rays, Scalar /*instance_weight*/);

    void end_batch();
};

} // namespace lagrange::raycasting
