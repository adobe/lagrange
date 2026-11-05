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

#include "../src/OccludedFacetSampler.h"
#include "../src/OccludedInstanceSampler.h"
#include "../src/occluded_sampler_common.h"

#include <lagrange/testing/common.h>
#include <lagrange/testing/create_test_mesh.h>

#include <lagrange/Attribute.h>
#include <lagrange/IndexedAttribute.h>
#include <lagrange/raycasting/remove_occluded_facets.h>
#include <lagrange/raycasting/remove_occluded_instances.h>
#include <lagrange/scene/filter_instances.h>
#include <lagrange/utils/range.h>
#include <lagrange/views.h>
#include "../src/compute_graphcut_mask.h"
#include "../src/solve_graphcut_with_view_error.h"

#include <catch2/benchmark/catch_benchmark.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <algorithm>
#include <any>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <future>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

TEST_CASE("weighted sampling batch callbacks", "[raycasting][sampling_batch]")
{
    using Scalar = double;
    std::vector<Scalar> weights = {1, 2, 0};
    std::vector<size_t> processed;
    std::vector<uint64_t> allocated;
    std::vector<Scalar> active;
    uint64_t finishes = 0;
    auto weight = [&](size_t i) { return weights[i]; };
    auto process = [&](size_t i, uint64_t rays, Scalar w) {
        processed.push_back(i);
        allocated.push_back(rays);
        active.push_back(w);
    };
    auto finish = [&] { ++finishes; };

    SECTION("weights allocate rays and skip inactive instances")
    {
        lagrange::raycasting::detail::run_sampling_batch<Scalar>(
            90,
            weights.size(),
            weight,
            process,
            finish);
        REQUIRE(processed == std::vector<size_t>{0, 1});
        REQUIRE(allocated == std::vector<uint64_t>{30, 60});
        REQUIRE(active == std::vector<Scalar>{1, 2});
        REQUIRE(finishes == 1);
    }

    SECTION("no instances or active weight does not finalize")
    {
        lagrange::raycasting::detail::run_sampling_batch<Scalar>(90, 0, weight, process, finish);
        weights = {0, 0, 0};
        lagrange::raycasting::detail::run_sampling_batch<Scalar>(
            90,
            weights.size(),
            weight,
            process,
            finish);
        REQUIRE(processed.empty());
        REQUIRE(finishes == 0);
    }

    SECTION("allocations rounded to zero still finalize an active batch")
    {
        weights = {1, 1, 1};
        lagrange::raycasting::detail::run_sampling_batch<Scalar>(
            1,
            weights.size(),
            weight,
            process,
            finish);
        REQUIRE(processed.empty());
        REQUIRE(finishes == 1);
    }

    SECTION("independent rounding preserves actual versus requested counts")
    {
        weights = {1, 1, 1};
        lagrange::raycasting::detail::run_sampling_batch<Scalar>(
            2,
            weights.size(),
            weight,
            process,
            finish);
        REQUIRE(allocated == std::vector<uint64_t>{1, 1, 1});
        REQUIRE(finishes == 1);
    }

    SECTION("processing exceptions do not finalize the batch")
    {
        REQUIRE_THROWS_AS(
            lagrange::raycasting::detail::run_sampling_batch<Scalar>(
                90,
                weights.size(),
                weight,
                [](size_t, uint64_t, Scalar) { throw std::runtime_error("processing failed"); },
                finish),
            std::runtime_error);
        REQUIRE(finishes == 0);
    }
}

TEST_CASE("graph cut mask", "[raycasting][graph_cut]")
{
    using namespace lagrange::raycasting;

    SECTION("zero smoothness thresholds scores independently")
    {
        const std::vector<double> scores = {0.0, 0.1, 1.0};
        const std::vector<std::pair<uint64_t, uint64_t>> edges = {{0, 1}, {1, 2}};

        const auto mask = compute_graphcut_mask(scores, 0.1, edges, 0.0);

        REQUIRE(mask == std::vector<uint8_t>{0, 1, 1});
    }

    SECTION("smoothness favors a coherent label")
    {
        const std::vector<double> scores = {0.5, 2.0};
        const std::vector<std::pair<uint64_t, uint64_t>> edges = {{0, 1}};

        const auto mask = compute_graphcut_mask(scores, 1.0, edges, 2.0);

        REQUIRE(mask == std::vector<uint8_t>{1, 1});
    }
}

TEST_CASE("graph cut view-error solve", "[raycasting][graph_cut]")
{
    using namespace lagrange::raycasting;

    const std::vector<double> scores = {0.0, 0.5, 1.0};
    const std::vector<std::pair<uint64_t, uint64_t>> edges = {{0, 1}, {1, 2}};

    OccludedFacetViewErrorOptions options;
    options.num_threshold_iterations = 4;
    options.smoothness_tradeoff = std::nullopt;

    SECTION("all-kept baseline is a successful no-removal solution")
    {
        const auto result = solve_graphcut_with_view_error(
            scores,
            edges,
            [](lagrange::span<const uint8_t> keep_mask,
               lagrange::span<const uint64_t>,
               lagrange::span<double> errors) {
                const double error = std::all_of(
                                         keep_mask.begin(),
                                         keep_mask.end(),
                                         [](uint8_t keep) { return keep != 0; })
                                         ? 0.0
                                         : 1.0;
                std::fill(errors.begin(), errors.end(), error);
            },
            options);

        REQUIRE(result.within_error_budget);
        REQUIRE(result.removed_fraction == 0.0);
        REQUIRE(result.threshold == 0.0);
    }

    SECTION("free removal selects a positive threshold")
    {
        const auto result = solve_graphcut_with_view_error(
            scores,
            edges,
            [](lagrange::span<const uint8_t>,
               lagrange::span<const uint64_t>,
               lagrange::span<double> errors) { std::fill(errors.begin(), errors.end(), 0.0); },
            options);

        REQUIRE(result.within_error_budget);
        REQUIRE(result.removed_fraction > 0.0);
    }

    SECTION("zero scores still permit free removal")
    {
        const std::vector<double> zero_scores(3, 0.0);
        const auto result = solve_graphcut_with_view_error(
            zero_scores,
            edges,
            [](lagrange::span<const uint8_t>,
               lagrange::span<const uint64_t>,
               lagrange::span<double> errors) { std::fill(errors.begin(), errors.end(), 0.0); },
            options);

        REQUIRE(result.within_error_budget);
        REQUIRE(result.removed_fraction == 1.0);
    }

    SECTION("feasible zero-removal probes advance the search")
    {
        const std::vector<double> positive_scores = {0.6, 1.0};
        const auto result = solve_graphcut_with_view_error(
            positive_scores,
            {},
            [](lagrange::span<const uint8_t> keep_mask,
               lagrange::span<const uint64_t>,
               lagrange::span<double> errors) {
                std::fill(errors.begin(), errors.end(), keep_mask[1] ? 0.0 : 1.0);
            },
            options);

        REQUIRE(result.within_error_budget);
        REQUIRE(result.removed_fraction == 0.5);
        REQUIRE(
            compute_graphcut_mask(
                positive_scores,
                result.threshold,
                {},
                result.graphcut_smoothness) == std::vector<uint8_t>{0, 1});
    }

    SECTION("largest finite scores yield finite representable parameters")
    {
        const std::vector<double> extreme_scores = {0.0, std::numeric_limits<double>::max()};
        const auto result = solve_graphcut_with_view_error(
            extreme_scores,
            {},
            [](lagrange::span<const uint8_t>,
               lagrange::span<const uint64_t>,
               lagrange::span<double> errors) { std::fill(errors.begin(), errors.end(), 0.0); },
            options);

        REQUIRE(result.within_error_budget);
        REQUIRE(std::isfinite(result.threshold));
        REQUIRE(result.removed_fraction == 0.5);
        REQUIRE(
            compute_graphcut_mask(
                extreme_scores,
                result.threshold,
                {},
                result.graphcut_smoothness) == std::vector<uint8_t>{0, 1});
    }

    SECTION("uniform positive scores still permit free removal")
    {
        const std::vector<double> uniform_scores(3, 1.0);
        const auto result = solve_graphcut_with_view_error(
            uniform_scores,
            edges,
            [](lagrange::span<const uint8_t>,
               lagrange::span<const uint64_t>,
               lagrange::span<double> errors) { std::fill(errors.begin(), errors.end(), 0.0); },
            options);

        REQUIRE(result.within_error_budget);
        REQUIRE(result.removed_fraction == 1.0);
    }
}

TEST_CASE("graph cut view-error smoothing budget", "[raycasting][graph_cut]")
{
    using namespace lagrange::raycasting;
    const std::vector<double> scores = {0.0, 1.0};
    const std::vector<std::pair<uint64_t, uint64_t>> edges = {{0, 1}};
    OccludedFacetViewErrorOptions options;
    options.num_threshold_iterations = 12;
    options.graphcut_smoothness_candidates = {1.0};
    options.smoothness_tradeoff = 1.0;
    auto evaluate = [](lagrange::span<const uint8_t> keep,
                       lagrange::span<const uint64_t>,
                       lagrange::span<double> errors) {
        std::fill(errors.begin(), errors.end(), keep[1] ? 0.0 : 1.0);
    };

    SECTION("whole removal budget can restore zero-score neighbors")
    {
        const auto result = solve_graphcut_with_view_error(scores, edges, evaluate, options);
        REQUIRE(result.within_error_budget);
        REQUIRE(result.graphcut_smoothness == 1.0);
        REQUIRE(result.removed_fraction == 0.0);
        REQUIRE(result.num_isolated_facets == 0);
        REQUIRE(
            compute_graphcut_mask(scores, result.threshold, edges, result.graphcut_smoothness) ==
            std::vector<uint8_t>{1, 1});
    }

    SECTION("half the removal budget rejects excessive restoration")
    {
        options.smoothness_tradeoff = 0.5;
        const auto result = solve_graphcut_with_view_error(scores, edges, evaluate, options);
        REQUIRE(result.within_error_budget);
        REQUIRE(result.graphcut_smoothness == 0.0);
        REQUIRE(result.removed_fraction == 0.5);
        REQUIRE(result.num_isolated_facets == 1);
    }
}

TEST_CASE("graph cut view-error verifies and refines smoothing", "[raycasting][graph_cut]")
{
    using namespace lagrange::raycasting;
    const std::vector<double> scores = {0.1, 0.4, 1.0, 0.9};
    const std::vector<std::pair<uint64_t, uint64_t>> edges = {{1, 2}};
    OccludedFacetViewErrorOptions options;
    options.num_threshold_iterations = 12;
    options.num_views = 2;
    options.graphcut_smoothness_candidates = {0.5};
    bool tracked_one_view = false;
    bool refined_two_views = false;
    bool full_view_failure = false;
    bool full_view_success = false;
    auto evaluate = [&](lagrange::span<const uint8_t> keep,
                        lagrange::span<const uint64_t> views,
                        lagrange::span<double> errors) {
        tracked_one_view |= views.size() == 1 && views[0] == 0;
        refined_two_views |= views.size() == 2 && views[0] == 0 && views[1] == 1;
        full_view_failure |= views.empty() && !keep[2] && keep[3];
        full_view_success |= views.empty() && !keep[0] && keep[1] && keep[2] && keep[3];
        for (size_t i = 0; i < errors.size(); ++i) {
            const auto view = views.empty() ? i : views[i];
            errors[i] = view == 0 ? (keep[3] ? 0.005 : 1.0) : (keep[2] ? 0.0 : 1.0);
        }
    };

    const auto result = solve_graphcut_with_view_error(scores, edges, evaluate, options, 1);
    REQUIRE(result.within_error_budget);
    REQUIRE(result.graphcut_smoothness == 0.5);
    REQUIRE(result.removed_fraction == 0.25);
    REQUIRE(result.num_isolated_facets == 1);
    REQUIRE(tracked_one_view);
    REQUIRE(refined_two_views);
    REQUIRE(full_view_failure);
    REQUIRE(full_view_success);
    REQUIRE(
        compute_graphcut_mask(scores, result.threshold, edges, result.graphcut_smoothness) ==
        std::vector<uint8_t>{0, 1, 1, 1});
}

TEST_CASE("adaptive occluded-facet sampler adjacency", "[raycasting][graph_cut]")
{
    using Scalar = float;
    using Index = uint32_t;
    using Scene = lagrange::scene::SimpleScene<Scalar, Index, 3>;

    lagrange::SurfaceMesh<Scalar, Index> mesh;
    mesh.add_vertex({0, 0, 0});
    mesh.add_vertex({1, 0, 0});
    mesh.add_vertex({0, 1, 0});
    mesh.add_triangle(0, 1, 2);

    Scene scene;
    scene.add_mesh(std::move(mesh));
    scene.add_instance({0});

    lagrange::raycasting::OccludedFacetSamplerOptions options;
    options.adaptive = lagrange::raycasting::AdaptiveOptions{};

    using Sampler = lagrange::raycasting::OccludedFacetSampler<Scalar, Index>;
    STATIC_REQUIRE_FALSE(std::is_copy_constructible_v<Sampler>);
    STATIC_REQUIRE_FALSE(std::is_copy_assignable_v<Sampler>);
    STATIC_REQUIRE(std::is_move_constructible_v<Sampler>);
    STATIC_REQUIRE(std::is_move_assignable_v<Sampler>);
    Sampler sampler(scene, options);
    REQUIRE(sampler.num_rays_cast(0) == 0);
    REQUIRE(sampler.num_retired() == 0);
    REQUIRE(sampler.num_facets() == 1);
    REQUIRE(sampler.instances().size() == 1);
    sampler.run_batch(20);
    REQUIRE(sampler.num_rays_cast(0) > 0);
    REQUIRE(sampler.visibility_measure(0) > 0.0);

    options.adaptive->num_adaptive_per_cosine = 0;
    LA_REQUIRE_THROWS(Sampler(scene, options));
    options.adaptive->num_adaptive_per_cosine = 1;
    options.adaptive->vmf_kappa = 0;
    LA_REQUIRE_THROWS(Sampler(scene, options));
    options.adaptive.reset();
    Sampler plain(scene, options);
    plain.run_batch(20);
    REQUIRE(plain.num_rays_cast(0) > 0);
    REQUIRE(plain.visibility_measure(0) > 0.0);

    lagrange::raycasting::RemoveOccludedFacetsOptions remove_options;
    REQUIRE_FALSE(remove_options.graphcut_smoothness.has_value());
    REQUIRE(remove_options.sampler_options.num_rays == 1600000000ULL);
    REQUIRE(remove_options.sampler_options.batch_size == 50000000ULL);
    remove_options.sampler_options.num_rays = 100;
    remove_options.sampler_options.batch_size = 100;
    lagrange::raycasting::OccludedFacetViewErrorOptions solver_options;
    solver_options.num_threshold_iterations = 4;
    solver_options.smoothness_tradeoff = std::nullopt;
    lagrange::ProgressCallback progress;
    const auto [result, params] =
        lagrange::raycasting::remove_occluded_facets_with_view_error<Scalar, Index>(
            scene,
            [](lagrange::span<const uint8_t> keep_mask,
               lagrange::span<const uint64_t>,
               lagrange::span<double> errors) {
                const bool all_kept =
                    std::all_of(keep_mask.begin(), keep_mask.end(), [](uint8_t keep) {
                        return keep != 0;
                    });
                std::fill(errors.begin(), errors.end(), all_kept ? 0.0 : 1.0);
            },
            remove_options,
            solver_options,
            progress);
    REQUIRE(params.within_error_budget);
    REQUIRE(result.get_mesh(0).get_num_facets() == 1);

    remove_options.sampler_options.max_visibility_threshold = 0.0;
    const auto threshold_only = lagrange::raycasting::remove_occluded_facets<Scalar, Index>(
        scene,
        remove_options,
        progress);
    REQUIRE(threshold_only.get_mesh(0).get_num_facets() == 1);
}

TEST_CASE("occluded-instance sampler counters and retirement", "[raycasting][occlusion]")
{
    using Scalar = float;
    using Index = uint32_t;
    using namespace lagrange;
    using namespace lagrange::raycasting;
    using Sampler = OccludedInstanceSampler<Scalar, Index>;
    STATIC_REQUIRE_FALSE(std::is_copy_constructible_v<Sampler>);
    STATIC_REQUIRE_FALSE(std::is_copy_assignable_v<Sampler>);
    STATIC_REQUIRE(std::is_move_constructible_v<Sampler>);
    STATIC_REQUIRE(std::is_move_assignable_v<Sampler>);

    SurfaceMesh<Scalar, Index> mesh;
    mesh.add_vertex({0, 0, 0});
    mesh.add_vertex({1, 0, 0});
    mesh.add_vertex({0, 1, 0});
    mesh.add_triangle(0, 1, 2);
    scene::SimpleScene<Scalar, Index, 3> source;
    source.add_mesh(std::move(mesh));
    source.add_instance({0});
    OccludedInstanceSamplerOptions options;
    options.threshold = 0.4;
    options.size_influence = 0;
    Sampler sampler(source, options, [](Index, Index) { return false; });
    REQUIRE(sampler.num_instances() == 1);
    REQUIRE(sampler.num_retired() == 0);
    REQUIRE(sampler.num_rays_cast(0, 0) == 0);
    REQUIRE(sampler.visibility_measure(0, 0) == 0.0);
    sampler.run_batch(16);
    REQUIRE(sampler.num_rays_cast(0, 0) == 16);
    REQUIRE(sampler.visibility_measure(0, 0) == 1.0);
    REQUIRE(sampler.is_visible(0, 0));
    REQUIRE(sampler.num_retired() == 1);
    REQUIRE(sampler.progress() == std::make_pair(Index{1}, uint64_t{16}));
    sampler.run_batch(16);
    REQUIRE(sampler.num_rays_cast(0, 0) == 16);
    LA_REQUIRE_THROWS(sampler.visibility_measure(1, 0));
    LA_REQUIRE_THROWS(sampler.num_rays_cast(0, 1));
    options.confidence = 1;
    LA_REQUIRE_THROWS(Sampler(source, options));
}

TEST_CASE("public instance example workflow keeps cancelled measures", "[raycasting][occlusion]")
{
    using Scalar = float;
    using Index = uint32_t;
    using namespace lagrange;
    using namespace lagrange::raycasting;
    SurfaceMesh<Scalar, Index> mesh;
    mesh.add_vertex({0, 0, 0});
    mesh.add_vertex({1, 0, 0});
    mesh.add_vertex({0, 1, 0});
    mesh.add_triangle(0, 1, 2);
    scene::SimpleScene<Scalar, Index, 3> source;
    source.add_mesh(std::move(mesh));
    source.add_instance({0});
    source.add_instance({0});
    source.add_mesh(SurfaceMesh<Scalar, Index>{});
    RemoveOccludedInstancesOptions options;
    options.sampler_options.threshold = 0.4;
    options.sampler_options.size_influence = 0;
    options.sampler_options.confidence = std::nextafter(1.0, 0.0);
    options.estimate_options.num_rays = 0;
    options.estimate_options.batch_size = 16;
    options.estimate_options.until_converged = GENERATE(false, true);
    std::atomic_bool cancel{true};
    ProgressCallback progress;
    size_t predicate_calls = 0;
    const auto measures = estimate_occluded_instance_measures<Scalar, Index>(
        source,
        options,
        progress,
        [&](Index, Index) {
            ++predicate_calls;
            return false;
        },
        &cancel);
    REQUIRE(measures.size() == 2);
    REQUIRE(measures[0] == std::vector<double>{1.0, 1.0});
    REQUIRE(measures[1].empty());
    const auto calls_after_sampling = predicate_calls;
    for (double threshold : {0.0, 0.4}) {
        const auto output = scene::filter_instances(source, [&](Index mi, Index ii) {
            return measures[mi][ii] >= threshold;
        });
        REQUIRE(output.compute_num_instances() == 2);
        REQUIRE(predicate_calls == calls_after_sampling);
    }
    REQUIRE(source.compute_num_instances() == 2);
    REQUIRE(source.get_num_meshes() == 2);
}

TEST_CASE("public facet example workflow reapplies cancelled estimates", "[raycasting][occlusion]")
{
    using Scalar = float;
    using Index = uint32_t;
    using namespace lagrange;
    using namespace lagrange::raycasting;
    SurfaceMesh<Scalar, Index> mesh;
    mesh.add_vertex({0, 0, 0});
    mesh.add_vertex({1, 0, 0});
    mesh.add_vertex({0, 1, 0});
    mesh.add_vertex({2, 0, 0});
    mesh.add_vertex({3, 0, 0});
    mesh.add_vertex({4, 0, 0});
    mesh.add_triangle(0, 1, 2);
    mesh.add_triangle(3, 4, 5);
    scene::SimpleScene<Scalar, Index, 3> source;
    source.add_mesh(std::move(mesh));
    source.add_instance({0});
    source.add_instance({0});
    OccludedFacetSamplerOptions sampling;
    sampling.adaptive =
        GENERATE(std::optional<AdaptiveOptions>{}, std::optional{AdaptiveOptions{}});
    sampling.size_influence = 0;
    sampling.max_visibility_threshold = 0.4;
    sampling.confidence = std::nextafter(1.0, 0.0);
    sampling.num_rays = 0;
    sampling.batch_size = 32;
    std::atomic_bool cancel{true};
    ProgressCallback progress;
    size_t predicate_calls = 0;
    const OccludedFacetsFilter<Scalar, Index> filter(
        source,
        sampling,
        progress,
        [&](Index, Index) {
            ++predicate_calls;
            return false;
        },
        &cancel);
    const auto calls_after_sampling = predicate_calls;
    OccludedFacetFilterOptions options;
    options.visibility_threshold = 0.4;
    options.instancing = InstancingPolicy::FlattenInstances;
    options.visibility_attribute_name = "visibility";
    options.keep_attribute_name = "keep";
    const auto classified = filter.apply(options);
    REQUIRE(classified.get_num_meshes() == 2);
    REQUIRE(classified.compute_num_instances() == 2);
    for (Index mi = 0; mi < classified.get_num_meshes(); ++mi) {
        const auto& output = classified.get_mesh(mi);
        REQUIRE(output.get_num_facets() == 2);
        REQUIRE(output.get_attribute<uint8_t>("keep").get_all()[0] == 1);
        REQUIRE(output.get_attribute<uint8_t>("keep").get_all()[1] == 0);
        REQUIRE(output.get_attribute<Scalar>("visibility").get_all()[0] > 0.4f);
        REQUIRE(output.get_attribute<Scalar>("visibility").get_all()[1] == 0.0f);
    }
    options.visibility_threshold = 0.2;
    options.graphcut_smoothness = 0.01;
    const auto retuned = filter.apply(options);
    REQUIRE(retuned.get_num_meshes() == classified.get_num_meshes());
    for (Index mi = 0; mi < retuned.get_num_meshes(); ++mi) {
        REQUIRE(retuned.get_mesh(mi).get_num_facets() == classified.get_mesh(mi).get_num_facets());
        REQUIRE(retuned.get_mesh(mi).get_attribute<uint8_t>("keep").get_all()[1] == 0);
    }
    options.visibility_threshold = 0.4;
    options.graphcut_smoothness.reset();
    options.visibility_attribute_name.reset();
    options.keep_attribute_name.reset();
    const auto pruned = filter.apply(options);
    REQUIRE(pruned.get_num_meshes() == 2);
    REQUIRE(pruned.get_mesh(0).get_num_facets() == 1);
    REQUIRE(pruned.get_mesh(1).get_num_facets() == 1);
    REQUIRE(predicate_calls == calls_after_sampling);
    REQUIRE(source.get_num_meshes() == 1);
    REQUIRE(source.get_mesh(0).get_num_facets() == 2);
    REQUIRE_FALSE(source.get_mesh(0).has_attribute("visibility"));
    REQUIRE_FALSE(source.get_mesh(0).has_attribute("keep"));
}

TEST_CASE("facet removal preserves measures and user attributes", "[raycasting][graph_cut]")
{
    using Scalar = float;
    using Index = uint32_t;
    using namespace lagrange::raycasting;
    lagrange::SurfaceMesh<Scalar, Index> mesh;
    mesh.add_vertex({0, 0, 0});
    mesh.add_vertex({14, 0, 0});
    mesh.add_vertex({0, 1, 0});
    mesh.add_vertex({0, 10, 0});
    mesh.add_vertex({66, 10, 0});
    mesh.add_vertex({0, 11, 0});
    mesh.add_triangle(0, 1, 2);
    mesh.add_triangle(3, 4, 5);
    const std::vector<Scalar> values = {3.0f, 4.0f};
    constexpr auto attribute_name = "@remove_occluded_facets_measure";
    mesh.create_attribute<Scalar>(
        attribute_name,
        lagrange::AttributeElement::Facet,
        lagrange::AttributeUsage::Scalar,
        1,
        values);
    lagrange::scene::SimpleScene<Scalar, Index, 3> scene;
    scene.add_mesh(std::move(mesh));
    scene.add_instance({0});
    RemoveOccludedFacetsOptions options;
    options.sampler_options.adaptive = std::nullopt;
    options.sampler_options.size_influence = 1.0;
    options.sampler_options.max_visibility_threshold = 0.35;
    options.sampler_options.num_rays = 200;
    options.sampler_options.batch_size = 200;
    lagrange::ProgressCallback progress;

    auto prepared = OccludedFacetsFilter(scene, options.sampler_options, progress);
    for (auto policy :
         {InstancingPolicy::Max, InstancingPolicy::Average, InstancingPolicy::FlattenInstances}) {
        options.instancing = policy;
        const auto estimated =
            estimate_facet_visibility(scene, "public_measure", options, progress);
        const auto public_values =
            lagrange::attribute_vector_view<Scalar>(estimated.get_mesh(0), "public_measure");
        REQUIRE(
            static_cast<double>(public_values(0)) <
            options.sampler_options.max_visibility_threshold);

        for (const auto graphcut_smoothness :
             {std::optional<double>{}, std::optional<double>{0.0}}) {
            options.graphcut_smoothness = graphcut_smoothness;
            const auto result = remove_occluded_facets(scene, options, progress);
            REQUIRE(result.get_num_meshes() == 1);
            REQUIRE(result.get_mesh(0).get_num_facets() == 2);
            const auto retained =
                lagrange::attribute_vector_view<Scalar>(result.get_mesh(0), attribute_name);
            REQUIRE(retained(0) == values[0]);
            REQUIRE(retained(1) == values[1]);
            REQUIRE(!result.get_mesh(0).has_attribute("public_measure"));
            OccludedFacetFilterOptions filter_options;
            filter_options.visibility_threshold = options.sampler_options.max_visibility_threshold;
            filter_options.graphcut_smoothness = graphcut_smoothness;
            filter_options.instancing = policy;
            const auto reused = prepared.apply(filter_options);
            REQUIRE(reused.get_mesh(0).get_num_facets() == 2);
            REQUIRE(
                lagrange::attribute_vector_view<Scalar>(reused.get_mesh(0), attribute_name)(0) ==
                values[0]);
            filter_options.visibility_attribute_name = "exported_measure";
            const auto visible = prepared.apply(filter_options);
            REQUIRE(visible.get_mesh(0).get_num_facets() == 2);
            REQUIRE(
                lagrange::attribute_vector_view<Scalar>(visible.get_mesh(0), "exported_measure")(
                    0) < filter_options.visibility_threshold);
            filter_options.keep_attribute_name = "keep";
            const auto classified = prepared.apply(filter_options);
            REQUIRE(classified.get_mesh(0).get_num_facets() == 2);
            REQUIRE(classified.get_mesh(0).get_attribute<uint8_t>("keep").get_all()[0] == 1);
            REQUIRE(classified.get_mesh(0).get_attribute<uint8_t>("keep").get_all()[1] == 1);
            REQUIRE_FALSE(scene.get_mesh(0).has_attribute("exported_measure"));
            REQUIRE_FALSE(scene.get_mesh(0).has_attribute("keep"));
        }
    }
}

TEST_CASE("facet removal deduplicates shared-edge neighbors", "[raycasting][graph_cut]")
{
    using Scalar = float;
    using Index = uint32_t;
    using namespace lagrange::raycasting;
    lagrange::SurfaceMesh<Scalar, Index> mesh;
    mesh.add_vertex({-0.5f, -0.5f, 0.95f});
    mesh.add_vertex({0.5f, -0.5f, 0.95f});
    mesh.add_vertex({-0.5f, 0.5f, 1.05f});
    mesh.add_triangle(0, 1, 2);
    mesh.add_triangle(0, 2, 1);
    const std::vector<Index> ids = {0, 1};
    mesh.create_attribute<Index>(
        "facet_id",
        lagrange::AttributeElement::Facet,
        lagrange::AttributeUsage::Scalar,
        1,
        ids);
    lagrange::scene::SimpleScene<Scalar, Index, 3> scene;
    scene.add_mesh(std::move(mesh));
    scene.add_instance({0});
    scene.add_mesh(lagrange::testing::create_test_cube<Scalar, Index>());
    scene.add_instance({1});
    RemoveOccludedFacetsOptions options;
    options.sampler_options.adaptive = std::nullopt;
    options.sampler_options.size_influence = 0.0;
    options.sampler_options.max_visibility_threshold = 0.35;
    options.sampler_options.confidence = std::nextafter(1.0, 0.0);
    options.sampler_options.num_rays = 1400;
    options.sampler_options.batch_size = 1400;
    lagrange::ProgressCallback progress;

    const auto estimated = estimate_facet_visibility(scene, "score", options, progress);
    const auto scores = lagrange::attribute_vector_view<Scalar>(estimated.get_mesh(0), "score");
    CAPTURE(scores(0), scores(1));
    REQUIRE(scores(0) != scores(1));
    const double difference =
        std::abs(static_cast<double>(scores(0)) - static_cast<double>(scores(1)));
    const double threshold =
        (static_cast<double>(scores(0)) + static_cast<double>(scores(1))) / 2.0 - difference / 8.0;
    auto prepared = OccludedFacetsFilter(scene, options.sampler_options, progress);
    REQUIRE(prepared.get_max_visibility_threshold() == 0.35);
    REQUIRE(threshold <= prepared.get_max_visibility_threshold());


    const Index kept_facet = scores(0) > scores(1) ? 0 : 1;
    OccludedFacetFilterOptions filter_options;
    filter_options.visibility_threshold = threshold;
    for (const auto weight :
         {std::optional<double>{difference / 4.0},
          std::optional<double>{difference},
          std::optional<double>{difference / 4.0},
          std::optional<double>{0.0},
          std::optional<double>{}}) {
        filter_options.graphcut_smoothness = weight;
        const auto reused = prepared.apply(filter_options);
        const auto& filtered = reused.get_mesh(0);
        if (weight && *weight == difference) {
            REQUIRE(filtered.get_num_facets() == 2);
            const auto retained = lagrange::attribute_vector_view<Index>(filtered, "facet_id");
            REQUIRE(retained(0) == 0);
            REQUIRE(retained(1) == 1);
        } else {
            REQUIRE(filtered.get_num_facets() == 1);
            REQUIRE(lagrange::attribute_vector_view<Index>(filtered, "facet_id")(0) == kept_facet);
        }
    }
}

TEST_CASE("facet filtering reuses a borrowed source scene", "[raycasting][graph_cut]")
{
    using Scalar = float;
    using Index = uint32_t;
    using namespace lagrange::raycasting;
    using Scene = lagrange::scene::SimpleScene<Scalar, Index, 3>;
    lagrange::SurfaceMesh<Scalar, Index> mesh;
    mesh.add_vertex({0, 0, 0});
    mesh.add_vertex({14, 0, 0});
    mesh.add_vertex({0, 1, 0});
    mesh.add_vertex({0, 10, 0});
    mesh.add_vertex({66, 10, 0});
    mesh.add_vertex({0, 11, 0});
    mesh.add_triangle(0, 1, 2);
    mesh.add_triangle(3, 4, 5);
    const std::vector<Index> ids = {0, 1};
    mesh.create_attribute<Index>(
        "facet_id",
        lagrange::AttributeElement::Facet,
        lagrange::AttributeUsage::Scalar,
        1,
        ids);
    Scene scene;
    scene.add_mesh(std::move(mesh));
    scene.add_instance({0});
    scene.ref_instance(0, 0).user_data = std::string("source payload");
    OccludedFacetSamplerOptions sampler_options;
    sampler_options.adaptive = std::nullopt;
    sampler_options.size_influence = 1.0;
    sampler_options.max_visibility_threshold = 0.4;
    sampler_options.num_rays = 200;
    sampler_options.batch_size = 200;
    lagrange::ProgressCallback progress;
    uint64_t predicate_calls = 0;
    auto prepared =
        OccludedFacetsFilter<Scalar, Index>(scene, sampler_options, progress, [&](Index, Index) {
            ++predicate_calls;
            return true;
        });
    REQUIRE(prepared.get_max_visibility_threshold() == 0.4);
    REQUIRE(predicate_calls > 0);
    const auto calls_after_prepare = predicate_calls;
    OccludedFacetFilterOptions options;
    options.visibility_threshold = 0.4;

    SECTION("member operations match the free-function wrappers")
    {
        using Prepared = OccludedFacetsFilter<Scalar, Index>;
        auto direct = Prepared(scene, sampler_options, progress);
        REQUIRE(direct.get_max_visibility_threshold() == prepared.get_max_visibility_threshold());


        options.graphcut_smoothness = 0.02;
        RemoveOccludedFacetsOptions remove_options;
        remove_options.sampler_options = sampler_options;
        remove_options.graphcut_smoothness = options.graphcut_smoothness;
        const auto member_result = direct.apply(options);
        const auto wrapper_result = remove_occluded_facets(scene, remove_options, progress);
        REQUIRE(member_result.get_num_meshes() == wrapper_result.get_num_meshes());
        REQUIRE(
            member_result.get_mesh(0).get_num_facets() ==
            wrapper_result.get_mesh(0).get_num_facets());
        REQUIRE(
            lagrange::attribute_vector_view<Index>(member_result.get_mesh(0), "facet_id")(0) ==
            lagrange::attribute_vector_view<Index>(wrapper_result.get_mesh(0), "facet_id")(0));

        const auto member_measures = direct.estimate_visibility("measure");
        const auto wrapper_measures =
            estimate_facet_visibility(scene, "measure", remove_options, progress);
        const auto member_values =
            lagrange::attribute_vector_view<Scalar>(member_measures.get_mesh(0), "measure");
        const auto wrapper_values =
            lagrange::attribute_vector_view<Scalar>(wrapper_measures.get_mesh(0), "measure");
        REQUIRE(member_values.size() == wrapper_values.size());
        for (auto f : lagrange::range(member_values.size())) {
            REQUIRE(member_values(f) == wrapper_values(f));
        }
    }

    SECTION("all four apply modes classify once and remap exported visibility")
    {
        for (auto policy :
             {InstancingPolicy::Max,
              InstancingPolicy::Average,
              InstancingPolicy::FlattenInstances}) {
            options.instancing = policy;
            const auto measured = prepared.estimate_visibility("measure", policy);
            const auto measures =
                lagrange::attribute_vector_view<Scalar>(measured.get_mesh(0), "measure");
            for (auto weight :
                 {std::optional<double>{}, std::optional<double>{0}, std::optional<double>{0.02}}) {
                options.graphcut_smoothness = weight;
                options.visibility_attribute_name = std::nullopt;
                options.keep_attribute_name = std::nullopt;
                const auto pruned = prepared.apply(options);
                const auto pruned_ids =
                    lagrange::attribute_vector_view<Index>(pruned.get_mesh(0), "facet_id");
                REQUIRE(pruned_ids.size() == 1);
                REQUIRE(pruned_ids(0) == 1);
                for (bool visibility : {false, true}) {
                    for (bool mask : {false, true}) {
                        options.visibility_attribute_name =
                            visibility ? std::make_optional<std::string>("measure") : std::nullopt;
                        options.keep_attribute_name =
                            mask ? std::make_optional<std::string>("keep") : std::nullopt;
                        const auto result = prepared.apply(options);
                        REQUIRE(result.get_num_meshes() == 1);
                        REQUIRE(result.get_num_instances(0) == 1);
                        const auto& output = result.get_mesh(0);
                        REQUIRE(output.get_num_facets() == (mask ? 2 : 1));
                        REQUIRE(output.get_num_vertices() == scene.get_mesh(0).get_num_vertices());
                        REQUIRE(output.has_attribute("measure") == visibility);
                        REQUIRE(output.has_attribute("keep") == mask);
                        if (visibility) {
                            const auto& attribute = output.get_attribute<Scalar>("measure");
                            REQUIRE(
                                attribute.get_element_type() == lagrange::AttributeElement::Facet);
                            REQUIRE(attribute.get_usage() == lagrange::AttributeUsage::Scalar);
                            REQUIRE(attribute.get_num_channels() == 1);
                            const auto values = attribute.get_all();
                            REQUIRE(values.size() == (mask ? 2 : 1));
                            REQUIRE(values[0] == measures(mask ? 0 : 1));
                            if (mask) REQUIRE(values[1] == measures(1));
                        }
                        if (mask) {
                            const auto& attribute = output.get_attribute<uint8_t>("keep");
                            REQUIRE(
                                attribute.get_element_type() == lagrange::AttributeElement::Facet);
                            REQUIRE(attribute.get_usage() == lagrange::AttributeUsage::Scalar);
                            REQUIRE(attribute.get_num_channels() == 1);
                            const auto keep = attribute.get_all();
                            REQUIRE(keep.size() == 2);
                            REQUIRE(keep[0] == 0);
                            REQUIRE(keep[1] == 1);
                            const auto output_ids =
                                lagrange::attribute_vector_view<Index>(output, "facet_id");
                            std::vector<Index> selected;
                            for (size_t f = 0; f < keep.size(); ++f) {
                                if (keep[f]) selected.push_back(output_ids(f));
                            }
                            REQUIRE(selected == std::vector<Index>{pruned_ids(0)});
                            const auto output_facets = output.get_corner_to_vertex().get_all();
                            const auto source_facets =
                                scene.get_mesh(0).get_corner_to_vertex().get_all();
                            REQUIRE(
                                std::equal(
                                    output_facets.begin(),
                                    output_facets.end(),
                                    source_facets.begin()));
                        }
                    }
                }
                REQUIRE(predicate_calls == calls_after_prepare);
                REQUIRE(scene.get_mesh(0).get_num_facets() == 2);
                REQUIRE_FALSE(scene.get_mesh(0).has_attribute("measure"));
                REQUIRE_FALSE(scene.get_mesh(0).has_attribute("keep"));
            }
        }
    }

    SECTION("export collisions are errors before any output mutation")
    {
        options.visibility_attribute_name = "duplicate";
        options.keep_attribute_name = "duplicate";
        LA_REQUIRE_THROWS(prepared.apply(options));
        options.visibility_attribute_name = "facet_id";
        options.keep_attribute_name = std::nullopt;
        LA_REQUIRE_THROWS(prepared.apply(options));
        options.visibility_attribute_name = std::nullopt;
        options.keep_attribute_name = "facet_id";
        LA_REQUIRE_THROWS(prepared.apply(options));
        options.visibility_attribute_name = "new_measure";
        LA_REQUIRE_THROWS(prepared.apply(options));
        REQUIRE(scene.get_mesh(0).get_num_facets() == 2);
        REQUIRE_FALSE(scene.get_mesh(0).has_attribute("new_measure"));
        REQUIRE(lagrange::attribute_vector_view<Index>(scene.get_mesh(0), "facet_id")(0) == 0);
        LA_REQUIRE_THROWS(prepared.estimate_visibility("facet_id"));
    }

    SECTION("present empty names follow mesh attribute creation semantics")
    {
        options.visibility_attribute_name = "";
        const auto visible = prepared.apply(options);
        REQUIRE(visible.get_mesh(0).has_attribute(""));
        REQUIRE(visible.get_mesh(0).get_num_facets() == 1);
        REQUIRE(prepared.estimate_visibility("").get_mesh(0).has_attribute(""));
        options.visibility_attribute_name = std::nullopt;
        options.keep_attribute_name = "";
        const auto masked = prepared.apply(options);
        REQUIRE(masked.get_mesh(0).get_num_facets() == 2);
        REQUIRE(masked.get_mesh(0).get_attribute<uint8_t>("").get_all()[0] == 0);
        options.visibility_attribute_name = "";
        LA_REQUIRE_THROWS(prepared.apply(options));
    }

    SECTION("thresholds and weights do not resample or cumulatively prune")
    {
        for (auto weight :
             {std::optional<double>{}, std::optional<double>{0}, std::optional<double>{0.02}}) {
            options.graphcut_smoothness = weight;
            options.visibility_threshold = 0.4;
            const auto high = prepared.apply(options);
            REQUIRE(high.get_mesh(0).get_num_facets() == 1);
            REQUIRE(lagrange::attribute_vector_view<Index>(high.get_mesh(0), "facet_id")(0) == 1);
            options.visibility_threshold = 0.3;
            const auto low = prepared.apply(options);
            REQUIRE(low.get_mesh(0).get_num_facets() == 2);
            REQUIRE(
                std::any_cast<std::string>(low.get_instance(0, 0).user_data) == "source payload");
            options.visibility_threshold = 0;
            REQUIRE(prepared.apply(options).get_mesh(0).get_num_facets() == 2);
            REQUIRE(predicate_calls == calls_after_prepare);
        }
        REQUIRE(scene.get_mesh(0).get_num_facets() == 2);
        REQUIRE_FALSE(scene.get_mesh(0).has_attribute("measure"));
        REQUIRE(scene.get_instance(0, 0).transform.translation().x() == 0);
        REQUIRE(std::any_cast<std::string>(scene.get_instance(0, 0).user_data) == "source payload");
        const auto source_ids =
            lagrange::attribute_vector_view<Index>(scene.get_mesh(0), "facet_id");
        REQUIRE(source_ids(0) == 0);
        REQUIRE(source_ids(1) == 1);
    }

    SECTION("threshold and smoothness validation")
    {
        for (double threshold :
             {-0.1,
              0.41,
              std::numeric_limits<double>::infinity(),
              std::numeric_limits<double>::quiet_NaN()}) {
            options.visibility_threshold = threshold;
            LA_REQUIRE_THROWS(prepared.apply(options));
        }
        options.visibility_threshold = 0.4;
        for (double weight :
             {-0.1,
              std::numeric_limits<double>::infinity(),
              std::numeric_limits<double>::quiet_NaN()}) {
            options.graphcut_smoothness = weight;
            LA_REQUIRE_THROWS(prepared.apply(options));
        }
    }

    SECTION("const operations initialize graph data lazily without sampling")
    {
        const auto& read_only = prepared;
        const auto annotated = read_only.estimate_visibility("measure");
        REQUIRE(annotated.get_mesh(0).get_num_facets() == 2);
        for (auto weight :
             {std::optional<double>{},
              std::optional<double>{0},
              std::optional<double>{0.02},
              std::optional<double>{0.03}}) {
            options.graphcut_smoothness = weight;
            REQUIRE(read_only.apply(options).get_mesh(0).get_num_facets() == 1);
        }
        REQUIRE(predicate_calls == calls_after_prepare);
    }

    SECTION("move-only filters transfer their cached graph data")
    {
        using Filter = OccludedFacetsFilter<Scalar, Index>;
        STATIC_REQUIRE_FALSE(std::is_default_constructible_v<Filter>);
        STATIC_REQUIRE_FALSE(std::is_copy_constructible_v<Filter>);
        STATIC_REQUIRE_FALSE(std::is_copy_assignable_v<Filter>);
        STATIC_REQUIRE(std::is_move_constructible_v<Filter>);
        STATIC_REQUIRE(std::is_move_assignable_v<Filter>);

        options.graphcut_smoothness = 0.02;
        REQUIRE(prepared.apply(options).get_mesh(0).get_num_facets() == 1);
        auto moved = std::move(prepared);
        REQUIRE(moved.apply(options).get_mesh(0).get_num_facets() == 1);
        Filter assigned(scene, sampler_options, progress);
        assigned = std::move(moved);
        REQUIRE(assigned.apply(options).get_mesh(0).get_num_facets() == 1);
        REQUIRE(predicate_calls == calls_after_prepare);
    }

    SECTION("concurrent first positive cuts share the same cold cache")
    {
        options.graphcut_smoothness = 0.02;
        std::promise<void> start;
        const auto ready = start.get_future().share();
        std::vector<std::future<Scene>> results;
        for (int i = 0; i < 4; ++i) {
            results.push_back(std::async(std::launch::async, [&] {
                ready.wait();
                return prepared.apply(options);
            }));
        }
        start.set_value();
        for (auto& result : results) REQUIRE(result.get().get_mesh(0).get_num_facets() == 1);
        REQUIRE(predicate_calls == calls_after_prepare);
    }
}

TEST_CASE("prepared facet filtering defers instancing policy", "[raycasting][graph_cut]")
{
    using Scalar = double;
    using Index = uint32_t;
    using namespace lagrange::raycasting;
    lagrange::SurfaceMesh<Scalar, Index> triangle;
    triangle.add_vertex({-0.5, -0.5, 1});
    triangle.add_vertex({0.5, -0.5, 1});
    triangle.add_vertex({-0.5, 0.5, 1});
    triangle.add_triangle(0, 1, 2);
    lagrange::scene::SimpleScene<Scalar, Index, 3> scene;
    scene.add_mesh(std::move(triangle));
    scene.add_instance({0});
    scene.add_instance({0});
    scene.ref_instance(0, 1).transform.translation().z() = -0.2;
    scene.ref_instance(0, 0).user_data = std::string("top");
    scene.ref_instance(0, 1).user_data = std::string("inside");
    scene.add_mesh(lagrange::testing::create_test_cube<Scalar, Index>());
    scene.add_instance({1});
    OccludedFacetSamplerOptions sampler_options;
    sampler_options.adaptive = std::nullopt;
    sampler_options.size_influence = 0;
    sampler_options.max_visibility_threshold = 0.4;
    lagrange::ProgressCallback progress;
    sampler_options.num_rays = 4000;
    sampler_options.batch_size = 4000;
    auto prepared = OccludedFacetsFilter(scene, sampler_options, progress);

    OccludedFacetFilterOptions options;
    options.visibility_threshold = 0.3;
    for (auto weight :
         {std::optional<double>{}, std::optional<double>{0}, std::optional<double>{0.03}}) {
        options.graphcut_smoothness = weight;
        for (auto policy :
             {InstancingPolicy::Max,
              InstancingPolicy::Average,
              InstancingPolicy::FlattenInstances}) {
            options.instancing = policy;
            const auto result = prepared.apply(options);
            Index triangle_instances = 0;
            for (auto mi : lagrange::range(result.get_num_meshes())) {
                if (result.get_mesh(mi).get_num_vertices() != 3) continue;
                triangle_instances += result.get_num_instances(mi);
                for (auto ii : lagrange::range(result.get_num_instances(mi))) {
                    const auto& instance = result.get_instance(mi, ii);
                    const auto tag = std::any_cast<std::string>(instance.user_data);
                    REQUIRE((tag == "top" || tag == "inside"));
                    if (policy == InstancingPolicy::FlattenInstances) REQUIRE(tag == "top");
                }
            }
            REQUIRE(
                triangle_instances == (policy == InstancingPolicy::Max       ? 2
                                       : policy == InstancingPolicy::Average ? 0
                                                                             : 1));
            options.visibility_attribute_name = "visibility";
            options.keep_attribute_name = "keep";
            const auto annotated = prepared.apply(options);
            const auto measured = prepared.estimate_visibility("visibility", policy);
            REQUIRE(annotated.get_num_meshes() == measured.get_num_meshes());
            REQUIRE(
                annotated.get_num_meshes() ==
                (policy == InstancingPolicy::FlattenInstances ? 3 : 2));
            Index retained_instances = 0;
            Index selected_instances = 0;
            for (auto mi : lagrange::range(annotated.get_num_meshes())) {
                const auto& output = annotated.get_mesh(mi);
                REQUIRE(output.has_attribute("visibility"));
                REQUIRE(output.has_attribute("keep"));
                const auto exported = output.get_attribute<Scalar>("visibility").get_all();
                const auto expected_measures =
                    measured.get_mesh(mi).get_attribute<Scalar>("visibility").get_all();
                REQUIRE(exported.size() == expected_measures.size());
                REQUIRE(std::equal(exported.begin(), exported.end(), expected_measures.begin()));
                if (output.get_num_vertices() != 3) continue;
                REQUIRE(output.get_num_facets() == 1);
                retained_instances += annotated.get_num_instances(mi);
                if (output.get_attribute<uint8_t>("keep").get_all()[0]) {
                    selected_instances += annotated.get_num_instances(mi);
                }
            }
            REQUIRE(retained_instances == 2);
            REQUIRE(selected_instances == triangle_instances);
            options.visibility_attribute_name = std::nullopt;
            options.keep_attribute_name = std::nullopt;
            REQUIRE(scene.get_mesh(0).get_num_facets() == 1);
            REQUIRE_FALSE(scene.get_mesh(0).has_attribute("visibility"));
            REQUIRE_FALSE(scene.get_mesh(0).has_attribute("keep"));
        }
    }
}

TEST_CASE("facet keep export retains initially zero-facet instances", "[raycasting][graph_cut]")
{
    using Scalar = double;
    using Index = uint32_t;
    using namespace lagrange;
    using namespace lagrange::raycasting;
    SurfaceMesh<Scalar, Index> triangle;
    triangle.add_vertex({0, 0, 0});
    triangle.add_vertex({1, 0, 0});
    triangle.add_vertex({0, 1, 0});
    triangle.add_triangle(0, 1, 2);
    SurfaceMesh<Scalar, Index> vertices_only;
    vertices_only.add_vertex({2, 3, 4});
    vertices_only.add_vertex({5, 6, 7});
    const std::vector<Scalar> markers = {17, 23};
    vertices_only.create_attribute<Scalar>(
        "marker",
        AttributeElement::Vertex,
        1,
        AttributeUsage::Scalar,
        markers);
    vertices_only.create_attribute<Scalar>(
        "empty_collision",
        AttributeElement::Facet,
        1,
        AttributeUsage::Scalar);
    scene::SimpleScene<Scalar, Index, 3> source;
    source.add_mesh(std::move(triangle));
    source.add_instance({0});
    source.add_mesh(std::move(vertices_only));
    source.add_instance({1});
    source.add_instance({1});
    source.ref_instance(1, 0).transform.translation().x() = 5;
    source.ref_instance(1, 1).transform.translation().x() = -5;
    source.ref_instance(1, 0).user_data = std::string("vertex-a");
    source.ref_instance(1, 1).user_data = std::string("vertex-b");
    const auto original_positions = source.get_mesh(1).get_vertex_to_position().get_all();
    const std::vector<Scalar> positions(original_positions.begin(), original_positions.end());
    OccludedFacetSamplerOptions sampling;
    sampling.adaptive = std::nullopt;
    sampling.size_influence = 0;
    sampling.max_visibility_threshold = 0.2;
    sampling.num_rays = 100;
    sampling.batch_size = 100;
    ProgressCallback progress;
    const OccludedFacetsFilter prepared(source, sampling, progress);
    OccludedFacetFilterOptions options;
    options.visibility_threshold = 0.1;
    for (auto policy :
         {InstancingPolicy::Max, InstancingPolicy::Average, InstancingPolicy::FlattenInstances}) {
        options.instancing = policy;
        const auto measured = prepared.estimate_visibility("visibility", policy);
        REQUIRE(measured.get_num_meshes() == 1);
        REQUIRE(measured.compute_num_instances() == 1);
        REQUIRE(prepared.estimate_visibility("empty_collision", policy).get_num_meshes() == 1);
        for (auto weight :
             {std::optional<double>{}, std::optional<double>{0}, std::optional<double>{0.01}}) {
            options.graphcut_smoothness = weight;
            for (bool visibility : {false, true}) {
                options.visibility_attribute_name =
                    visibility ? std::make_optional<std::string>("visibility") : std::nullopt;
                options.keep_attribute_name = "keep";
                const auto result = prepared.apply(options);
                REQUIRE(
                    result.get_num_meshes() ==
                    (policy == InstancingPolicy::FlattenInstances ? 3 : 2));
                REQUIRE(result.compute_num_instances() == 3);
                Index empty_instances = 0;
                std::vector<std::string> tags;
                for (auto mi : range(result.get_num_meshes())) {
                    const auto& output = result.get_mesh(mi);
                    REQUIRE(output.has_attribute("visibility") == visibility);
                    const auto& keep = output.get_attribute<uint8_t>("keep");
                    REQUIRE(keep.get_num_elements() == output.get_num_facets());
                    REQUIRE(keep.get_num_channels() == 1);
                    REQUIRE(keep.get_element_type() == AttributeElement::Facet);
                    REQUIRE(keep.get_usage() == AttributeUsage::Scalar);
                    if (visibility) {
                        const auto& attribute = output.get_attribute<Scalar>("visibility");
                        REQUIRE(attribute.get_num_elements() == output.get_num_facets());
                        REQUIRE(attribute.get_num_channels() == 1);
                        REQUIRE(attribute.get_element_type() == AttributeElement::Facet);
                        REQUIRE(attribute.get_usage() == AttributeUsage::Scalar);
                    }
                    if (output.get_num_facets() != 0) continue;
                    REQUIRE(output.get_num_vertices() == 2);
                    REQUIRE(keep.get_all().empty());
                    if (visibility)
                        REQUIRE(output.get_attribute<Scalar>("visibility").get_all().empty());
                    const auto actual_positions = output.get_vertex_to_position().get_all();
                    REQUIRE(
                        std::equal(
                            actual_positions.begin(),
                            actual_positions.end(),
                            positions.begin()));
                    const auto actual_markers = output.get_attribute<Scalar>("marker").get_all();
                    REQUIRE(
                        std::equal(actual_markers.begin(), actual_markers.end(), markers.begin()));
                    empty_instances += result.get_num_instances(mi);
                    for (auto ii : range(result.get_num_instances(mi))) {
                        const auto& instance = result.get_instance(mi, ii);
                        const auto tag = std::any_cast<std::string>(instance.user_data);
                        const Index source_instance = tag == "vertex-a" ? 0 : 1;
                        REQUIRE(instance.transform.matrix().isApprox(
                            source.get_instance(1, source_instance).transform.matrix()));
                        tags.push_back(tag);
                    }
                }
                REQUIRE(empty_instances == 2);
                std::sort(tags.begin(), tags.end());
                REQUIRE(tags == std::vector<std::string>{"vertex-a", "vertex-b"});
                options.keep_attribute_name = std::nullopt;
                const auto pruned = prepared.apply(options);
                REQUIRE(pruned.get_num_meshes() == 1);
                REQUIRE(pruned.compute_num_instances() == 1);
                REQUIRE(pruned.get_mesh(0).get_num_facets() == 1);
            }
            options.visibility_attribute_name = "empty_collision";
            options.keep_attribute_name = "keep";
            LA_REQUIRE_THROWS(prepared.apply(options));
            options.visibility_attribute_name = "visibility";
            options.keep_attribute_name = "empty_collision";
            LA_REQUIRE_THROWS(prepared.apply(options));
            options.keep_attribute_name = std::nullopt;
            options.visibility_attribute_name = "empty_collision";
            REQUIRE(prepared.apply(options).get_num_meshes() == 1);
        }
        const auto& original = source.get_mesh(1);
        REQUIRE(original.get_num_facets() == 0);
        REQUIRE(original.get_num_vertices() == 2);
        const auto actual_positions = original.get_vertex_to_position().get_all();
        REQUIRE(std::equal(actual_positions.begin(), actual_positions.end(), positions.begin()));
        REQUIRE_FALSE(original.has_attribute("visibility"));
        REQUIRE_FALSE(original.has_attribute("keep"));
        REQUIRE(original.get_attribute<Scalar>("empty_collision").get_all().empty());
        const auto original_markers = original.get_attribute<Scalar>("marker").get_all();
        REQUIRE(std::equal(original_markers.begin(), original_markers.end(), markers.begin()));
        REQUIRE(source.get_mesh(0).get_num_facets() == 1);
        REQUIRE(source.compute_num_instances() == 3);
        REQUIRE(source.get_num_instances(1) == 2);
        REQUIRE(source.get_instance(1, 0).transform.translation().x() == 5);
        REQUIRE(source.get_instance(1, 1).transform.translation().x() == -5);
        REQUIRE(std::any_cast<std::string>(source.get_instance(1, 0).user_data) == "vertex-a");
        REQUIRE(std::any_cast<std::string>(source.get_instance(1, 1).user_data) == "vertex-b");
        REQUIRE_FALSE(source.get_mesh(0).has_attribute("visibility"));
        REQUIRE_FALSE(source.get_mesh(0).has_attribute("keep"));
    }
}

TEST_CASE(
    "facet filters borrow live unowned buffers without modifying the source",
    "[raycasting][graph_cut]")
{
    using Scalar = float;
    using Index = uint32_t;
    using namespace lagrange;
    using namespace lagrange::raycasting;
    for (auto policy :
         {AttributeCopyPolicy::CopyIfExternal, AttributeCopyPolicy::KeepExternalPtr}) {
        std::vector<Scalar> vertices = {0, 0, 0, 1, 0, 0, 0, 1, 0};
        std::vector<Index> facets = {0, 1, 2};
        std::vector<Scalar> marker = {7};
        std::vector<Scalar> uv_values = {0, 0, 1, 0, 0, 1};
        std::vector<Index> uv_indices = {0, 1, 2};
        SurfaceMesh<Scalar, Index> mesh;
        mesh.wrap_as_vertices(vertices, 3);
        mesh.wrap_as_facets(facets, 1, 3);
        mesh.wrap_as_attribute<Scalar>(
            "marker",
            AttributeElement::Facet,
            AttributeUsage::Scalar,
            1,
            marker);
        mesh.wrap_as_indexed_attribute<Scalar>(
            "uv",
            AttributeUsage::UV,
            3,
            2,
            uv_values,
            uv_indices);
        mesh.ref_vertex_to_position().set_copy_policy(policy);
        mesh.ref_vertex_to_position().set_growth_policy(AttributeGrowthPolicy::SilentCopy);
        mesh.ref_corner_to_vertex().set_copy_policy(policy);
        mesh.ref_corner_to_vertex().set_growth_policy(AttributeGrowthPolicy::SilentCopy);
        mesh.ref_attribute<Scalar>("marker").set_copy_policy(policy);
        mesh.ref_attribute<Scalar>("marker").set_growth_policy(AttributeGrowthPolicy::SilentCopy);
        mesh.ref_indexed_attribute<Scalar>("uv").values().set_copy_policy(policy);
        mesh.ref_indexed_attribute<Scalar>("uv").values().set_growth_policy(
            AttributeGrowthPolicy::SilentCopy);
        mesh.ref_indexed_attribute<Scalar>("uv").indices().set_copy_policy(policy);
        mesh.ref_indexed_attribute<Scalar>("uv").indices().set_growth_policy(
            AttributeGrowthPolicy::SilentCopy);
        scene::SimpleScene<Scalar, Index, 3> scene;
        scene.add_mesh(std::move(mesh));
        scene.add_instance({0});
        OccludedFacetSamplerOptions sampling;
        sampling.adaptive = std::nullopt;
        sampling.max_visibility_threshold = 0.2;
        ProgressCallback progress;
        sampling.num_rays = 100;
        sampling.batch_size = 100;
        const OccludedFacetsFilter facet_filter(scene, sampling, progress);
        const auto measured = facet_filter.estimate_visibility("measure");
        REQUIRE(attribute_vector_view<Scalar>(measured.get_mesh(0), "measure")(0) > 0.2);
        OccludedFacetFilterOptions options;
        options.visibility_threshold = 0.1;
        for (auto weight :
             {std::optional<double>{}, std::optional<double>{0}, std::optional<double>{0.01}}) {
            options.graphcut_smoothness = weight;
            const auto result = facet_filter.apply(options);
            const auto& retained = result.get_mesh(0);
            REQUIRE(retained.get_num_facets() == 1);
            REQUIRE(retained.get_vertex_to_position().get_all()[0] == vertices[0]);
            REQUIRE(retained.get_corner_to_vertex().get_all()[0] == facets[0]);
            REQUIRE(retained.get_attribute<Scalar>("marker").get_all()[0] == marker[0]);
            REQUIRE(
                retained.get_indexed_attribute<Scalar>("uv").values().get_all()[0] == uv_values[0]);
            REQUIRE(
                retained.get_indexed_attribute<Scalar>("uv").indices().get_all()[0] ==
                uv_indices[0]);
        }
        const auto& original = scene.get_mesh(0);
        REQUIRE(original.get_vertex_to_position().is_external());
        REQUIRE(original.get_corner_to_vertex().is_external());
        REQUIRE(original.get_attribute<Scalar>("marker").is_external());
        REQUIRE(original.get_attribute<Scalar>("marker").get_copy_policy() == policy);
        REQUIRE(original.get_indexed_attribute<Scalar>("uv").values().is_external());
        REQUIRE(original.get_indexed_attribute<Scalar>("uv").indices().is_external());
        REQUIRE_FALSE(original.has_attribute("measure"));
        REQUIRE(vertices[0] == 0);
        REQUIRE(facets[0] == 0);
        REQUIRE(marker[0] == 7);
        REQUIRE(uv_values[0] == 0);
        REQUIRE(uv_indices[0] == 0);
    }
}

TEST_CASE("partial facet filtering preserves borrowed external buffers", "[raycasting][graph_cut]")
{
    using Scalar = float;
    using Index = uint32_t;
    using namespace lagrange;
    using namespace lagrange::raycasting;
    using Scene = scene::SimpleScene<Scalar, Index, 3>;
    std::vector<Scalar> vertices = {0, 0, 0, 14, 0, 0, 0, 1, 0, 0, 10, 0, 66, 10, 0, 0, 11, 0};
    std::vector<Index> facets = {0, 1, 2, 3, 4, 5};
    std::vector<Scalar> marker = {7, 11};
    std::vector<Scalar> uv_values = {0, 0, 1, 0, 0, 1, 0, 2, 1, 2, 0, 3};
    std::vector<Index> uv_indices = {0, 1, 2, 3, 4, 5};
    const auto original_vertices = vertices;
    const auto original_facets = facets;
    const auto original_marker = marker;
    const auto original_uv_values = uv_values;
    const auto original_uv_indices = uv_indices;
    SurfaceMesh<Scalar, Index> mesh;
    mesh.wrap_as_vertices(vertices, 6);
    mesh.wrap_as_facets(facets, 2, 3);
    mesh.wrap_as_attribute<Scalar>(
        "marker",
        AttributeElement::Facet,
        AttributeUsage::Scalar,
        1,
        marker);
    mesh.wrap_as_indexed_attribute<Scalar>("uv", AttributeUsage::UV, 6, 2, uv_values, uv_indices);
    mesh.ref_vertex_to_position().set_copy_policy(AttributeCopyPolicy::CopyIfExternal);
    mesh.ref_corner_to_vertex().set_copy_policy(AttributeCopyPolicy::CopyIfExternal);
    mesh.ref_attribute<Scalar>("marker").set_copy_policy(AttributeCopyPolicy::CopyIfExternal);
    mesh.ref_indexed_attribute<Scalar>("uv").values().set_copy_policy(
        AttributeCopyPolicy::CopyIfExternal);
    mesh.ref_indexed_attribute<Scalar>("uv").indices().set_copy_policy(
        AttributeCopyPolicy::CopyIfExternal);
    Scene source;
    source.add_mesh(std::move(mesh));
    source.add_instance({0});
    OccludedFacetSamplerOptions sampling;
    sampling.adaptive = std::nullopt;
    sampling.size_influence = 1.0;
    sampling.max_visibility_threshold = 0.4;
    ProgressCallback progress;
    sampling.num_rays = 200;
    sampling.batch_size = 200;
    const OccludedFacetsFilter facet_filter(source, sampling, progress);
    auto check_source = [&] {
        REQUIRE(vertices == original_vertices);
        REQUIRE(facets == original_facets);
        REQUIRE(marker == original_marker);
        REQUIRE(uv_values == original_uv_values);
        REQUIRE(uv_indices == original_uv_indices);
        REQUIRE(source.get_mesh(0).get_attribute<Scalar>("marker").is_external());
        REQUIRE(
            source.get_mesh(0).get_attribute<Scalar>("marker").get_copy_policy() ==
            AttributeCopyPolicy::CopyIfExternal);
    };

    SECTION("repeated pruning does not compact the source")
    {
        for (auto weight :
             {std::optional<double>{}, std::optional<double>{0}, std::optional<double>{0.02}}) {
            OccludedFacetFilterOptions options;
            options.visibility_threshold = 0.4;
            options.graphcut_smoothness = weight;
            const auto high = facet_filter.apply(options);
            REQUIRE(high.get_mesh(0).get_num_facets() == 1);
            REQUIRE(high.get_mesh(0).get_attribute<Scalar>("marker").get_all()[0] == 11);
            REQUIRE(high.get_mesh(0).get_corner_to_vertex().get_all()[0] == 3);
            REQUIRE(
                high.get_mesh(0).get_indexed_attribute<Scalar>("uv").indices().get_all()[0] == 3);
            REQUIRE_FALSE(high.get_mesh(0).get_attribute<Scalar>("marker").is_external());
            check_source();
            options.visibility_threshold = 0.3;
            const auto low = facet_filter.apply(options);
            REQUIRE(low.get_mesh(0).get_num_facets() == 2);
            REQUIRE(low.get_mesh(0).get_attribute<Scalar>("marker").get_all()[0] == 7);
            check_source();
        }
    }

    SECTION("attribute exports respect caller external-copy policies")
    {
        for (auto weight :
             {std::optional<double>{}, std::optional<double>{0}, std::optional<double>{0.02}}) {
            for (bool visibility : {false, true}) {
                for (bool mask : {false, true}) {
                    OccludedFacetFilterOptions options;
                    options.visibility_threshold = 0.4;
                    options.graphcut_smoothness = weight;
                    if (visibility) options.visibility_attribute_name = "visibility";
                    if (mask) options.keep_attribute_name = "keep";
                    const auto result = facet_filter.apply(options);
                    const auto& output = result.get_mesh(0);
                    REQUIRE(output.get_num_facets() == (mask ? 2 : 1));
                    REQUIRE(output.get_attribute<Scalar>("marker").get_all()[0] == (mask ? 7 : 11));
                    if (mask) {
                        const auto keep = output.get_attribute<uint8_t>("keep").get_all();
                        REQUIRE(keep[0] == 0);
                        REQUIRE(keep[1] == 1);
                    }
                    check_source();
                    REQUIRE_FALSE(source.get_mesh(0).has_attribute("visibility"));
                    REQUIRE_FALSE(source.get_mesh(0).has_attribute("keep"));
                }
            }
        }
        OccludedFacetFilterOptions options;
        options.visibility_threshold = 0.4;
        options.visibility_attribute_name = "visibility";
        options.keep_attribute_name = "marker";
        LA_REQUIRE_THROWS(facet_filter.apply(options));
        check_source();
    }

    SECTION("concurrent pruning leaves the source unchanged")
    {
        OccludedFacetFilterOptions options;
        options.visibility_threshold = 0.4;
        options.graphcut_smoothness = 0.02;
        std::promise<void> start;
        const auto gate = start.get_future().share();
        std::vector<std::future<Scene>> results;
        for (int i = 0; i < 4; ++i) {
            results.push_back(std::async(std::launch::async, [&facet_filter, options, gate] {
                gate.wait();
                return facet_filter.apply(options);
            }));
        }
        start.set_value();
        for (auto& result : results) {
            const auto filtered = result.get();
            REQUIRE(filtered.get_mesh(0).get_num_facets() == 1);
            REQUIRE(filtered.get_mesh(0).get_attribute<Scalar>("marker").get_all()[0] == 11);
        }
        check_source();
    }
}

TEST_CASE(
    "legacy facet helpers preserve external user attributes that prohibit copying",
    "[raycasting][graph_cut]")
{
    using Scalar = double;
    using Index = uint32_t;
    using namespace lagrange;
    using namespace lagrange::raycasting;
    auto triangle = [] {
        SurfaceMesh<Scalar, Index> mesh;
        mesh.add_vertex({0, 0, 0});
        mesh.add_vertex({1, 0, 0});
        mesh.add_vertex({0, 1, 0});
        mesh.add_triangle(0, 1, 2);
        return mesh;
    };
    std::vector<Scalar> marker = {7};
    std::vector<Scalar> unused_marker = {11};
    auto referenced = triangle();
    referenced.wrap_as_attribute<Scalar>(
        "marker",
        AttributeElement::Facet,
        AttributeUsage::Scalar,
        1,
        marker);
    referenced.ref_attribute<Scalar>("marker").set_copy_policy(
        AttributeCopyPolicy::ErrorIfExternal);
    auto unused = triangle();
    unused.wrap_as_attribute<Scalar>(
        "marker",
        AttributeElement::Facet,
        AttributeUsage::Scalar,
        1,
        unused_marker);
    unused.ref_attribute<Scalar>("marker").set_copy_policy(AttributeCopyPolicy::ErrorIfExternal);
    scene::SimpleScene<Scalar, Index, 3> scene;
    scene.add_mesh(std::move(referenced));
    scene.add_instance({0});
    scene.add_mesh(std::move(unused));
    RemoveOccludedFacetsOptions options;
    options.sampler_options.adaptive = std::nullopt;
    options.sampler_options.max_visibility_threshold = 0.2;
    options.sampler_options.num_rays = 100;
    options.sampler_options.batch_size = 100;
    ProgressCallback progress;
    const auto measured = estimate_facet_visibility(scene, "measure", options, progress);
    REQUIRE(measured.get_num_meshes() == 1);
    REQUIRE(attribute_vector_view<Scalar>(measured.get_mesh(0), "measure")(0) > 0.2);
    REQUIRE(measured.get_mesh(0).get_attribute<Scalar>("marker").get_all()[0] == 7);
    for (auto weight :
         {std::optional<double>{}, std::optional<double>{0}, std::optional<double>{0.01}}) {
        options.graphcut_smoothness = weight;
        const auto result = remove_occluded_facets(scene, options, progress);
        REQUIRE(result.get_num_meshes() == 1);
        REQUIRE(result.get_mesh(0).get_num_facets() == 1);
        const auto& retained = result.get_mesh(0).get_attribute<Scalar>("marker");
        REQUIRE(retained.get_all()[0] == 7);
        REQUIRE(retained.get_copy_policy() == AttributeCopyPolicy::ErrorIfExternal);
    }
    for (Index mi = 0; mi < scene.get_num_meshes(); ++mi) {
        const auto& attribute = scene.get_mesh(mi).get_attribute<Scalar>("marker");
        REQUIRE(attribute.is_external());
        REQUIRE(attribute.get_copy_policy() == AttributeCopyPolicy::ErrorIfExternal);
        REQUIRE(attribute.get_all()[0] == (mi == 0 ? 7 : 11));
        REQUIRE_FALSE(scene.get_mesh(mi).has_attribute("measure"));
    }
}

TEST_CASE("facet keep export does not bypass caller attribute policies", "[raycasting][graph_cut]")
{
    using Scalar = float;
    using Index = uint32_t;
    using namespace lagrange;
    using namespace lagrange::raycasting;
    SurfaceMesh<Scalar, Index> mesh;
    mesh.add_vertex({0, 0, 0});
    mesh.add_vertex({14, 0, 0});
    mesh.add_vertex({0, 1, 0});
    mesh.add_vertex({0, 10, 0});
    mesh.add_vertex({66, 10, 0});
    mesh.add_vertex({0, 11, 0});
    mesh.add_triangle(0, 1, 2);
    mesh.add_triangle(3, 4, 5);
    std::vector<Scalar> marker = {7, 11};
    mesh.wrap_as_attribute<Scalar>(
        "marker",
        AttributeElement::Facet,
        AttributeUsage::Scalar,
        1,
        marker);
    mesh.ref_attribute<Scalar>("marker").set_copy_policy(AttributeCopyPolicy::ErrorIfExternal);
    mesh.ref_attribute<Scalar>("marker").set_growth_policy(AttributeGrowthPolicy::ErrorIfExternal);
    scene::SimpleScene<Scalar, Index, 3> source;
    source.add_mesh(std::move(mesh));
    source.add_instance({0});
    OccludedFacetSamplerOptions sampling;
    sampling.adaptive = std::nullopt;
    sampling.size_influence = 1;
    sampling.max_visibility_threshold = 0.4;
    sampling.num_rays = 200;
    sampling.batch_size = 200;
    ProgressCallback progress;
    const OccludedFacetsFilter prepared(source, sampling, progress);
    OccludedFacetFilterOptions options;
    options.visibility_threshold = 0.4;
    for (auto weight :
         {std::optional<double>{}, std::optional<double>{0}, std::optional<double>{0.02}}) {
        options.graphcut_smoothness = weight;
        for (bool visibility : {false, true}) {
            options.visibility_attribute_name =
                visibility ? std::make_optional<std::string>("visibility") : std::nullopt;
            std::string keep_name = "keep";
            options.keep_attribute_name = keep_name;
            keep_name = "changed";
            const auto annotated = prepared.apply(options);
            const auto& output = annotated.get_mesh(0);
            REQUIRE(output.get_num_facets() == 2);
            REQUIRE(output.get_attribute<uint8_t>("keep").get_all()[0] == 0);
            REQUIRE(output.get_attribute<uint8_t>("keep").get_all()[1] == 1);
            REQUIRE(output.get_attribute<Scalar>("marker").is_external());
            REQUIRE(
                output.get_attribute<Scalar>("marker").get_copy_policy() ==
                AttributeCopyPolicy::ErrorIfExternal);
            options.keep_attribute_name = std::nullopt;
            LA_REQUIRE_THROWS(prepared.apply(options));
            REQUIRE(marker == std::vector<Scalar>{7, 11});
            REQUIRE(source.get_mesh(0).get_num_facets() == 2);
            REQUIRE_FALSE(source.get_mesh(0).has_attribute("visibility"));
            REQUIRE_FALSE(source.get_mesh(0).has_attribute("keep"));
            REQUIRE(
                source.get_mesh(0).get_attribute<Scalar>("marker").get_growth_policy() ==
                AttributeGrowthPolicy::ErrorIfExternal);
        }
    }
}

TEST_CASE("graph cut benchmarks", "[raycasting][graph_cut][!benchmark]")
{
    using namespace lagrange::raycasting;

    constexpr uint64_t num_facets = 10000;
    std::vector<double> scores(num_facets);
    std::vector<std::pair<uint64_t, uint64_t>> edges;
    edges.reserve(num_facets - 1);
    for (uint64_t index = 0; index < num_facets; ++index) {
        scores[index] = static_cast<double>(index % 101) / 100.0;
        if (index > 0) edges.emplace_back(index - 1, index);
    }

    BENCHMARK("graph-cut mask")
    {
        return compute_graphcut_mask(scores, 0.5, edges, 0.1);
    };

    OccludedFacetViewErrorOptions options;
    options.num_threshold_iterations = 12;
    options.num_views = 1;
    options.smoothness_tradeoff = std::nullopt;
    BENCHMARK("graph-cut error threshold sweep")
    {
        return solve_graphcut_with_view_error(
            scores,
            edges,
            [](lagrange::span<const uint8_t>,
               lagrange::span<const uint64_t>,
               lagrange::span<double> errors) { std::fill(errors.begin(), errors.end(), 0.0); },
            options);
    };
}
