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

#include <lagrange/raycasting/remove_occluded_instances.h>

#include "OccludedInstanceSampler.h"
#include "occluded_sampler_common.h"

#include <lagrange/Logger.h>
#include <lagrange/SurfaceMeshTypes.h>
#include <lagrange/scene/filter_instances.h>
#include <lagrange/utils/assert.h>
#include <lagrange/utils/range.h>
#include <lagrange/utils/warning.h>

#include <algorithm>
#include <atomic>
#include <limits>
#include <utility>
#include <vector>

namespace lagrange::raycasting {

using namespace detail;

namespace {

OccludedInstanceSamplerOptions legacy_sampler_options()
{
    OccludedInstanceSamplerOptions options;
    options.threshold = std::numeric_limits<double>::min();
    return options;
}

} // namespace

namespace {

template <typename Scalar, typename Index>
void run_sampler(
    OccludedInstanceSampler<Scalar, Index>& sampler,
    const OccludedInstanceEstimateOptions& options,
    ProgressCallback& progress,
    const std::atomic_bool* cancel)
{
    la_runtime_assert(options.batch_size > 0);
    la_runtime_assert(
        options.num_rays > 0 || cancel != nullptr || options.until_converged,
        "run_sampler needs at least one termination condition: num_rays>0, "
        "until_converged=true, or a non-null cancel flag");

    const Index total_instances = sampler.num_instances();
    lagrange::logger().info("Searching for occluded instances");
    progress.set_section("Searching for occluded instances");

    Index prev_retired = 0;
    uint64_t prev_total_rays = 0;
    bool has_previous_batch = false;
    while (true) {
        sampler.run_batch(options.batch_size);

        // This is the actual count: retired instances receive no rays, and weighted allocation may
        // differ slightly from the requested batch size.
        const auto [num_visible, total_rays] = sampler.progress();
        const Index num_retired = sampler.num_retired();
        lagrange::logger().info(
            "{}/{} instances visible, {} confidently retired ({} rays so far)",
            num_visible,
            total_instances,
            num_retired,
            total_rays);
        const float fraction =
            options.num_rays > 0
                ? std::min(
                      1.f,
                      static_cast<float>(total_rays) / static_cast<float>(options.num_rays))
                : (total_instances > 0
                       ? static_cast<float>(num_retired) / static_cast<float>(total_instances)
                       : 1.f);
        progress.update(fraction);

        if (num_retired == total_instances) {
            lagrange::logger().info("All instances confidently visible, stopping early");
            break;
        }
        if (total_rays == prev_total_rays) {
            lagrange::logger().info("No instance rays could be cast, stopping");
            break;
        }
        if (options.until_converged && has_previous_batch && num_retired == prev_retired) {
            lagrange::logger().info("Converged: no instances retired in last batch, stopping");
            break;
        }
        prev_retired = num_retired;
        prev_total_rays = total_rays;
        has_previous_batch = true;

        if (cancel != nullptr && cancel->load()) {
            lagrange::logger().info("Cancelled, using results so far");
            break;
        }
        if (options.num_rays > 0 && total_rays >= options.num_rays) break;
    }
}

} // namespace

template <typename Scalar, typename Index>
std::vector<std::vector<double>> estimate_occluded_instance_measures(
    const scene::SimpleScene<Scalar, Index, 3>& scene,
    const RemoveOccludedInstancesOptions& options,
    ProgressCallback& progress,
    function_ref<bool(Index mesh_index, Index instance_index)> is_occluder,
    const std::atomic_bool* cancel)
{
    OccludedInstanceSampler<Scalar, Index> sampler(scene, options.sampler_options, is_occluder);
    run_sampler(sampler, options.estimate_options, progress, cancel);

    std::vector<std::vector<double>> measures(scene.get_num_meshes());
    for (auto mi : lagrange::range(scene.get_num_meshes())) {
        measures[mi].resize(scene.get_num_instances(mi));
        for (auto ii : lagrange::range(scene.get_num_instances(mi))) {
            measures[mi][ii] = sampler.visibility_measure(mi, ii);
        }
    }
    return measures;
}

template <typename Scalar, typename Index>
scene::SimpleScene<Scalar, Index, 3> remove_occluded_instances(
    const scene::SimpleScene<Scalar, Index, 3>& scene,
    const RemoveOccludedInstancesOptions& options,
    ProgressCallback& progress,
    function_ref<bool(Index mesh_index, Index instance_index)> is_occluder,
    const std::atomic_bool* cancel)
{
    OccludedInstanceSampler<Scalar, Index> sampler(scene, options.sampler_options, is_occluder);
    run_sampler(sampler, options.estimate_options, progress, cancel);

    auto result = scene::filter_instances(scene, [&](Index mi, Index ii) {
        return sampler.is_visible(mi, ii);
    });
    lagrange::logger().info(
        "Filtered scene: {} meshes, {} instances",
        result.get_num_meshes(),
        result.compute_num_instances());
    return result;
}

// Deprecated overloads use the smallest positive threshold for "one escaped ray keeps" semantics.
// Zero would also keep instances with no escapes because the public comparison is inclusive.
LA_IGNORE_DEPRECATION_WARNING_BEGIN

template <typename Scalar, typename Index>
void estimate_occluded_instances(
    const scene::SimpleScene<Scalar, Index, 3>& scene,
    function_ref<void(Index mesh_index, Index instance_index)> callback,
    const OccludedInstanceEstimateOptions& options,
    ProgressCallback& progress,
    function_ref<bool(Index mesh_index, Index instance_index)> is_occluder,
    const std::atomic_bool* cancel)
{
    OccludedInstanceSampler<Scalar, Index> sampler(scene, legacy_sampler_options(), is_occluder);
    run_sampler(sampler, options, progress, cancel);
    for (auto mi : lagrange::range(scene.get_num_meshes())) {
        for (auto ii : lagrange::range(scene.get_num_instances(mi))) {
            if (!sampler.is_visible(mi, ii)) callback(mi, ii);
        }
    }
}

template <typename Scalar, typename Index>
scene::SimpleScene<Scalar, Index, 3> remove_occluded_instances(
    const scene::SimpleScene<Scalar, Index, 3>& scene,
    const OccludedInstanceEstimateOptions& options,
    ProgressCallback& progress,
    function_ref<bool(Index mesh_index, Index instance_index)> is_occluder,
    const std::atomic_bool* cancel)
{
    return remove_occluded_instances<Scalar, Index>(
        scene,
        RemoveOccludedInstancesOptions{legacy_sampler_options(), options},
        progress,
        is_occluder,
        cancel);
}

LA_IGNORE_DEPRECATION_WARNING_END

// clang-format off

#define LA_X_estimate_occluded_instance_measures(_, Scalar, Index)                              \
    template LA_RAYCASTING_API std::vector<std::vector<double>>                                 \
    estimate_occluded_instance_measures(                                                        \
        const scene::SimpleScene<Scalar, Index, 3>&,                                            \
        const RemoveOccludedInstancesOptions&,                                                  \
        ProgressCallback&,                                                                      \
        function_ref<bool(Index, Index)>,                                                       \
        const std::atomic_bool*);
LA_SURFACE_MESH_X(estimate_occluded_instance_measures, 0)

#define LA_X_remove_occluded_instances(_, Scalar, Index)                                        \
    template LA_RAYCASTING_API scene::SimpleScene<Scalar, Index, 3> remove_occluded_instances(  \
        const scene::SimpleScene<Scalar, Index, 3>&,                                            \
        const RemoveOccludedInstancesOptions&,                                                  \
        ProgressCallback&,                                                                      \
        function_ref<bool(Index, Index)>,                                                       \
        const std::atomic_bool*);
LA_SURFACE_MESH_X(remove_occluded_instances, 0)

LA_IGNORE_DEPRECATION_WARNING_BEGIN
#define LA_X_estimate_occluded_instances_deprecated(_, Scalar, Index)                           \
    template LA_RAYCASTING_API void estimate_occluded_instances(                                \
        const scene::SimpleScene<Scalar, Index, 3>&,                                            \
        function_ref<void(Index, Index)>,                                                       \
        const OccludedInstanceEstimateOptions&,                                                 \
        ProgressCallback&,                                                                      \
        function_ref<bool(Index, Index)>,                                                       \
        const std::atomic_bool*);
LA_SURFACE_MESH_X(estimate_occluded_instances_deprecated, 0)

#define LA_X_remove_occluded_instances_deprecated(_, Scalar, Index)                             \
    template LA_RAYCASTING_API scene::SimpleScene<Scalar, Index, 3> remove_occluded_instances(  \
        const scene::SimpleScene<Scalar, Index, 3>&,                                            \
        const OccludedInstanceEstimateOptions&,                                                 \
        ProgressCallback&,                                                                      \
        function_ref<bool(Index, Index)>,                                                       \
        const std::atomic_bool*);
LA_SURFACE_MESH_X(remove_occluded_instances_deprecated, 0)
LA_IGNORE_DEPRECATION_WARNING_END
// clang-format on

} // namespace lagrange::raycasting
