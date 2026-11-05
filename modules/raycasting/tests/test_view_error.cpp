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

#include "../src/compute_graphcut_mask.h"
#include "../src/solve_graphcut_with_view_error.h"

#include <lagrange/testing/common.h>

#include <lagrange/Attribute.h>
#include <lagrange/compute_facet_facet_adjacency.h>
#include <lagrange/raycasting/remove_occluded_facets.h>
#include <lagrange/utils/range.h>

#include <catch2/catch_approx.hpp>

#include <algorithm>
#include <any>
#include <cmath>
#include <future>
#include <limits>
#include <optional>
#include <string>
#include <utility>
#include <vector>

TEST_CASE("view-error solver validates all options before evaluation", "[raycasting][view_error]")
{
    using namespace lagrange::raycasting;
    const std::vector<double> scores = {1};
    size_t calls = 0;
    auto evaluate = [&](lagrange::span<const uint8_t>,
                        lagrange::span<const uint64_t>,
                        lagrange::span<double> errors) {
        ++calls;
        std::fill(errors.begin(), errors.end(), 0.0);
    };
    OccludedFacetViewErrorOptions options;
    const double inf = std::numeric_limits<double>::infinity();
    const double nan = std::numeric_limits<double>::quiet_NaN();
    SECTION("invalid budgets")
    {
        for (double value : {-1.0, inf, nan}) {
            options.max_view_error = value;
            LA_REQUIRE_THROWS(solve_graphcut_with_view_error(scores, {}, evaluate, options));
        }
    }
    SECTION("invalid explicit weights even with smoothing disabled")
    {
        options.smoothness_tradeoff = std::nullopt;
        for (double value : {0.0, -1.0, inf, nan}) {
            options.graphcut_smoothness_candidates = {1.0, value};
            LA_REQUIRE_THROWS(solve_graphcut_with_view_error(scores, {}, evaluate, options));
        }
    }
    SECTION("invalid iteration and view counts")
    {
        options.num_threshold_iterations = 0;
        LA_REQUIRE_THROWS(solve_graphcut_with_view_error(scores, {}, evaluate, options));
        options.num_threshold_iterations = 1;
        options.num_refinement_iterations = 0;
        LA_REQUIRE_THROWS(solve_graphcut_with_view_error(scores, {}, evaluate, options));
        options.num_refinement_iterations = 1;
        options.num_views = 0;
        LA_REQUIRE_THROWS(solve_graphcut_with_view_error(scores, {}, evaluate, options));
    }
    SECTION("invalid optional tradeoffs")
    {
        for (double value : {-0.1, 1.1, inf, nan}) {
            options.smoothness_tradeoff = value;
            LA_REQUIRE_THROWS(solve_graphcut_with_view_error(scores, {}, evaluate, options));
        }
    }
    REQUIRE(calls == 0);
}

TEST_CASE("view-error solver reports the returned mask on every path", "[raycasting][view_error]")
{
    using namespace lagrange::raycasting;
    const std::vector<double> scores = {1};
    OccludedFacetViewErrorOptions options;
    options.num_threshold_iterations = 4;
    options.num_views = 2;
    options.smoothness_tradeoff = std::nullopt;
    SECTION("failed baseline keeps an isolated facet and reports the actual error")
    {
        const auto result = solve_graphcut_with_view_error(
            scores,
            {},
            [](lagrange::span<const uint8_t>,
               lagrange::span<const uint64_t>,
               lagrange::span<double> errors) {
                errors[0] = 0.02;
                errors[1] = 0.1;
            },
            options);
        REQUIRE_FALSE(result.within_error_budget);
        REQUIRE(result.worst_view_error == 0.1);
        REQUIRE(result.num_isolated_facets == 1);
        REQUIRE(result.removed_fraction == 0);
        REQUIRE(result.threshold == 0);
        REQUIRE(result.graphcut_smoothness == 0);
        REQUIRE(
            compute_graphcut_mask(scores, result.threshold, {}, result.graphcut_smoothness) ==
            std::vector<uint8_t>{1});
    }
    SECTION("no feasible removal reports baseline rather than rejected endpoint error")
    {
        const auto result = solve_graphcut_with_view_error(
            scores,
            {},
            [](lagrange::span<const uint8_t> keep,
               lagrange::span<const uint64_t>,
               lagrange::span<double> errors) {
                std::fill(errors.begin(), errors.end(), keep[0] ? 0.005 : 1.0);
            },
            options);
        REQUIRE(result.within_error_budget);
        REQUIRE(result.worst_view_error == 0.005);
        REQUIRE(result.num_isolated_facets == 1);
        REQUIRE(result.removed_fraction == 0);
        REQUIRE(result.threshold == 0);
    }
    SECTION("exact-match budget allows free removal")
    {
        options.max_view_error = 0;
        const auto result = solve_graphcut_with_view_error(
            scores,
            {},
            [](lagrange::span<const uint8_t>,
               lagrange::span<const uint64_t>,
               lagrange::span<double> errors) { std::fill(errors.begin(), errors.end(), 0.0); },
            options);
        REQUIRE(result.within_error_budget);
        REQUIRE(result.worst_view_error == 0);
        REQUIRE(result.removed_fraction == 1);
        REQUIRE(result.num_isolated_facets == 0);
        REQUIRE(
            compute_graphcut_mask(scores, result.threshold, {}, result.graphcut_smoothness) ==
            std::vector<uint8_t>{0});
    }
    SECTION("invalid evaluator errors are rejected")
    {
        for (double error :
             {-1.0,
              std::numeric_limits<double>::infinity(),
              std::numeric_limits<double>::quiet_NaN()}) {
            LA_REQUIRE_THROWS(solve_graphcut_with_view_error(
                scores,
                {},
                [&](lagrange::span<const uint8_t>,
                    lagrange::span<const uint64_t>,
                    lagrange::span<double> errors) {
                    std::fill(errors.begin(), errors.end(), error);
                },
                options));
        }
    }
}

TEST_CASE("smoothness tradeoff includes its exact removal boundary", "[raycasting][view_error]")
{
    using namespace lagrange::raycasting;
    const std::vector<double> scores = {0.01, 0.02, 0.03, 0.4, 1.0};
    const std::vector<std::pair<uint64_t, uint64_t>> edges = {{3, 4}};
    OccludedFacetViewErrorOptions options;
    options.max_view_error = 0;
    options.num_threshold_iterations = 16;
    options.graphcut_smoothness_candidates = {0.5};
    for (double tradeoff : {std::nextafter(0.25, 0.0), 0.25, std::nextafter(0.25, 1.0)}) {
        options.smoothness_tradeoff = tradeoff;
        const auto result = solve_graphcut_with_view_error(
            scores,
            edges,
            [](lagrange::span<const uint8_t> keep,
               lagrange::span<const uint64_t>,
               lagrange::span<double> errors) {
                std::fill(errors.begin(), errors.end(), keep[4] ? 0.0 : 1.0);
            },
            options);
        const bool allow_one_fewer_removal = tradeoff >= 0.25;
        REQUIRE(result.within_error_budget);
        REQUIRE(result.worst_view_error == 0);
        REQUIRE(result.graphcut_smoothness == (allow_one_fewer_removal ? 0.5 : 0.0));
        REQUIRE(result.removed_fraction == Catch::Approx(allow_one_fewer_removal ? 0.6 : 0.8));
        REQUIRE(result.num_isolated_facets == (allow_one_fewer_removal ? 0 : 1));
        const auto expected = allow_one_fewer_removal ? std::vector<uint8_t>{0, 0, 0, 1, 1}
                                                      : std::vector<uint8_t>{0, 0, 0, 0, 1};
        REQUIRE(
            compute_graphcut_mask(scores, result.threshold, edges, result.graphcut_smoothness) ==
            expected);
    }
}

TEST_CASE("refinement includes the exact removal tradeoff boundary", "[raycasting][view_error]")
{
    using namespace lagrange::raycasting;
    const std::vector<double> scores = {0.01, 0.02, 0.1, 0.4, 1.0, 0.9};
    const std::vector<std::pair<uint64_t, uint64_t>> edges = {{3, 4}};
    OccludedFacetViewErrorOptions options;
    options.num_threshold_iterations = 16;
    options.num_views = 2;
    options.graphcut_smoothness_candidates = {0.5};
    for (double tradeoff : {std::nextafter(0.25, 0.0), 0.25}) {
        options.smoothness_tradeoff = tradeoff;
        bool full_view_failure = false;
        const auto result = solve_graphcut_with_view_error(
            scores,
            edges,
            [&](lagrange::span<const uint8_t> keep,
                lagrange::span<const uint64_t> views,
                lagrange::span<double> errors) {
                full_view_failure |= views.empty() && !keep[4] && keep[5];
                for (size_t i = 0; i < errors.size(); ++i) {
                    const auto view = views.empty() ? i : views[i];
                    errors[i] = view == 0 ? (keep[5] ? 0.005 : 1.0) : (keep[4] ? 0.0 : 1.0);
                }
            },
            options,
            1);
        const bool allow_one_fewer_removal = tradeoff >= 0.25;
        REQUIRE(full_view_failure);
        REQUIRE(result.within_error_budget);
        REQUIRE(result.worst_view_error == 0.005);
        REQUIRE(result.graphcut_smoothness == (allow_one_fewer_removal ? 0.5 : 0.0));
        REQUIRE(
            result.removed_fraction == Catch::Approx(allow_one_fewer_removal ? 0.5 : 4.0 / 6.0));
        REQUIRE(result.num_isolated_facets == (allow_one_fewer_removal ? 1 : 2));
        const auto expected = allow_one_fewer_removal ? std::vector<uint8_t>{0, 0, 0, 1, 1, 1}
                                                      : std::vector<uint8_t>{0, 0, 0, 0, 1, 1};
        REQUIRE(
            compute_graphcut_mask(scores, result.threshold, edges, result.graphcut_smoothness) ==
            expected);
    }
}

TEST_CASE(
    "zero smoothness tradeoff enables count-preserving label changes",
    "[raycasting][view_error]")
{
    using namespace lagrange::raycasting;
    const std::vector<double> scores = {0.1, 0.4, 1.0, 0.6};
    const std::vector<std::pair<uint64_t, uint64_t>> edges = {{1, 2}};
    OccludedFacetViewErrorOptions options;
    options.num_threshold_iterations = 16;
    options.graphcut_smoothness_candidates = {0.5};
    auto evaluate = [](lagrange::span<const uint8_t> keep,
                       lagrange::span<const uint64_t>,
                       lagrange::span<double> errors) {
        std::fill(errors.begin(), errors.end(), keep[2] && (keep[1] || keep[3]) ? 0.005 : 1.0);
    };
    options.smoothness_tradeoff = std::nullopt;
    const auto independent = solve_graphcut_with_view_error(scores, edges, evaluate, options);
    REQUIRE(independent.graphcut_smoothness == 0);
    REQUIRE(independent.removed_fraction == 0.5);
    REQUIRE(independent.num_isolated_facets == 2);
    REQUIRE(
        compute_graphcut_mask(
            scores,
            independent.threshold,
            edges,
            independent.graphcut_smoothness) == std::vector<uint8_t>{0, 0, 1, 1});

    options.smoothness_tradeoff = 0;
    const auto coherent = solve_graphcut_with_view_error(scores, edges, evaluate, options);
    REQUIRE(coherent.graphcut_smoothness == 0.5);
    REQUIRE(coherent.removed_fraction == independent.removed_fraction);
    REQUIRE(coherent.num_isolated_facets == 0);
    REQUIRE(coherent.worst_view_error == 0.005);
    REQUIRE(coherent.within_error_budget);
    REQUIRE(
        compute_graphcut_mask(scores, coherent.threshold, edges, coherent.graphcut_smoothness) ==
        std::vector<uint8_t>{0, 1, 1, 0});
}

TEST_CASE(
    "smoothness candidates are normalized without extending the sweep",
    "[raycasting][view_error]")
{
    using namespace lagrange::raycasting;
    const std::vector<double> scores = {0.1, 0.4, 1.0, 0.6};
    const std::vector<std::pair<uint64_t, uint64_t>> edges = {{1, 2}};
    OccludedFacetViewErrorOptions options;
    options.num_threshold_iterations = 16;
    options.smoothness_tradeoff = 0;
    auto solve = [&](const std::vector<double>& candidates) {
        options.graphcut_smoothness_candidates = candidates;
        std::vector<std::vector<uint8_t>> probes;
        const auto result = solve_graphcut_with_view_error(
            scores,
            edges,
            [&](lagrange::span<const uint8_t> keep,
                lagrange::span<const uint64_t>,
                lagrange::span<double> errors) {
                probes.emplace_back(keep.begin(), keep.end());
                std::fill(
                    errors.begin(),
                    errors.end(),
                    keep[2] && (keep[1] || keep[3]) ? 0.005 : 1.0);
            },
            options);
        return std::make_pair(result, probes);
    };
    const auto sorted = solve({0.5, 1.0});
    const auto duplicate = solve({1.0, 0.5, 0.5});
    REQUIRE(duplicate.first.graphcut_smoothness == sorted.first.graphcut_smoothness);
    REQUIRE(duplicate.first.threshold == sorted.first.threshold);
    REQUIRE(duplicate.second == sorted.second);
    REQUIRE(options.graphcut_smoothness_candidates == std::vector<double>{1.0, 0.5, 0.5});

    const auto plateau = solve({0.5, 0.01});
    REQUIRE(plateau.first.graphcut_smoothness == 0);
    REQUIRE(plateau.first.num_isolated_facets == 2);
    REQUIRE(sorted.first.num_isolated_facets == 0);
}

TEST_CASE(
    "refinement iterations are independent of primary search iterations",
    "[raycasting][view_error]")
{
    using namespace lagrange::raycasting;
    const std::vector<double> scores = {0.1, 0.4, 1.0, 0.9};
    const std::vector<std::pair<uint64_t, uint64_t>> edges = {{1, 2}};
    for (uint64_t refinements : {1, 5, 13}) {
        OccludedFacetViewErrorOptions options;
        options.num_threshold_iterations = 12;
        options.num_refinement_iterations = refinements;
        options.num_views = 2;
        options.graphcut_smoothness_candidates = {0.5};
        bool refining = false;
        uint64_t primary_probes = 0;
        uint64_t refinement_probes = 0;
        const auto result = solve_graphcut_with_view_error(
            scores,
            edges,
            [&](lagrange::span<const uint8_t> keep,
                lagrange::span<const uint64_t> views,
                lagrange::span<double> errors) {
                if (views.size() == 1) ++primary_probes;
                if (refining && views.size() == 2) ++refinement_probes;
                if (views.empty() && !keep[2] && keep[3]) refining = true;
                for (size_t i = 0; i < errors.size(); ++i) {
                    const auto view = views.empty() ? i : views[i];
                    errors[i] = view == 0 ? (keep[3] ? 0.005 : 1.0) : (keep[2] ? 0.0 : 1.0);
                }
            },
            options,
            1);
        REQUIRE(refining);
        REQUIRE(primary_probes == options.num_threshold_iterations);
        REQUIRE(refinement_probes == refinements);
        REQUIRE(result.within_error_budget);
        REQUIRE(result.worst_view_error == 0.005);
        REQUIRE(result.graphcut_smoothness == 0.5);
    }
}

TEST_CASE(
    "cached view-error filtering preserves global ordering and source state",
    "[raycasting][view_error]")
{
    using Scalar = double;
    using Index = uint32_t;
    using namespace lagrange;
    using namespace lagrange::raycasting;
    using Scene = scene::SimpleScene<Scalar, Index, 3>;
    SurfaceMesh<Scalar, Index> pair;
    pair.add_vertex({0, 0, 0});
    pair.add_vertex({1, 0, 0});
    pair.add_vertex({0, 1, 0});
    pair.add_vertex({10, 0, 0});
    pair.add_vertex({12, 0, 0});
    pair.add_vertex({10, 1, 0});
    pair.add_triangle(0, 1, 2);
    pair.add_triangle(3, 4, 5);
    const std::vector<Index> markers = {10, 20};
    pair.create_attribute<Index>(
        "marker",
        AttributeElement::Facet,
        1,
        AttributeUsage::Scalar,
        markers);
    SurfaceMesh<Scalar, Index> empty;
    empty.add_vertex({3, 4, 5});
    SurfaceMesh<Scalar, Index> triangle;
    triangle.add_vertex({0, 0, 0});
    triangle.add_vertex({1, 0, 0});
    triangle.add_vertex({0, 1, 0});
    triangle.add_triangle(0, 1, 2);
    Scene source;
    source.add_mesh(std::move(pair));
    source.add_instance({0});
    source.add_instance({0});
    source.add_mesh(std::move(empty));
    source.add_instance({1});
    source.add_instance({1});
    source.add_mesh(std::move(triangle));
    source.add_instance({2});
    source.ref_instance(0, 0).user_data = std::string("a");
    source.ref_instance(0, 1).user_data = std::string("b");
    source.ref_instance(2, 0).user_data = std::string("c");
    source.ref_instance(0, 0).transform.translation().x() = 1;
    source.ref_instance(0, 1).transform.translation().x() = 3;
    source.ref_instance(2, 0).transform.translation().x() = 5;
    OccludedFacetSamplerOptions sampling;
    sampling.adaptive = std::nullopt;
    sampling.size_influence = 1;
    sampling.max_visibility_threshold = 0.2;
    sampling.num_rays = 400;
    sampling.batch_size = 400;
    ProgressCallback progress;
    size_t occluder_calls = 0;
    const OccludedFacetsFilter<Scalar, Index> filter(source, sampling, progress, [&](Index, Index) {
        ++occluder_calls;
        return false;
    });
    const auto calls_after_sampling = occluder_calls;
    OccludedFacetViewErrorOptions options;
    options.num_threshold_iterations = 14;
    options.num_views = 2;
    options.smoothness_tradeoff = std::nullopt;
    auto evaluate = [](span<const uint8_t> keep, span<const uint64_t>, span<double> errors) {
        REQUIRE(keep.size() == 5);
        std::fill(errors.begin(), errors.end(), keep[4] ? 0.005 : 1.0);
    };
    auto check_source = [&] {
        REQUIRE(source.get_num_meshes() == 3);
        REQUIRE(source.compute_num_instances() == 5);
        REQUIRE(source.get_mesh(0).get_num_facets() == 2);
        REQUIRE(source.get_mesh(1).get_num_facets() == 0);
        REQUIRE(source.get_mesh(1).get_num_vertices() == 1);
        REQUIRE(source.get_mesh(2).get_num_facets() == 1);
        REQUIRE_FALSE(source.get_mesh(0).has_edges());
        const auto values = source.get_mesh(0).get_attribute<Index>("marker").get_all();
        REQUIRE(std::equal(values.begin(), values.end(), markers.begin()));
        REQUIRE(occluder_calls == calls_after_sampling);
    };
    auto check_partial = [&](const Scene& output) {
        REQUIRE(output.get_num_meshes() == 3);
        REQUIRE(output.compute_num_instances() == 3);
        for (Index mi = 0; mi < 3; ++mi) {
            REQUIRE(output.get_mesh(mi).get_num_facets() == 1);
            REQUIRE(output.get_num_instances(mi) == 1);
            const auto& instance = output.get_instance(mi, 0);
            REQUIRE(std::any_cast<std::string>(instance.user_data) == std::string(1, 'a' + mi));
            const auto& original = mi < 2 ? source.get_instance(0, mi) : source.get_instance(2, 0);
            REQUIRE(instance.transform.matrix().isApprox(original.transform.matrix()));
            if (mi < 2)
                REQUIRE(output.get_mesh(mi).get_attribute<Index>("marker").get_all()[0] == 20);
        }
    };
    SECTION("repeated solves use the same global scores and output mask")
    {
        const auto measured =
            filter.estimate_visibility("visibility", InstancingPolicy::FlattenInstances);
        std::vector<double> scores;
        for (Index mi = 0; mi < measured.get_num_meshes(); ++mi) {
            const auto values = measured.get_mesh(mi).get_attribute<Scalar>("visibility").get_all();
            scores.insert(scores.end(), values.begin(), values.end());
        }
        for (int repetition = 0; repetition < 2; ++repetition) {
            const auto [output, solved] = filter.apply_with_view_error(evaluate, options);
            check_partial(output);
            REQUIRE(solved.within_error_budget);
            REQUIRE(solved.worst_view_error == 0.005);
            REQUIRE(solved.removed_fraction == Catch::Approx(0.4));
            REQUIRE(solved.num_isolated_facets == 3);
            REQUIRE(solved.threshold > filter.get_max_visibility_threshold());
            REQUIRE(
                compute_graphcut_mask(scores, solved.threshold, {}, solved.graphcut_smoothness) ==
                std::vector<uint8_t>{0, 1, 0, 1, 1});
            OccludedFacetFilterOptions bounded;
            bounded.visibility_threshold = solved.threshold;
            LA_REQUIRE_THROWS(filter.apply(bounded));
        }
    }
    SECTION("all removed output is empty without changing the borrowed source")
    {
        const auto [output, solved] = filter.apply_with_view_error(
            [](span<const uint8_t>, span<const uint64_t>, span<double> errors) {
                std::fill(errors.begin(), errors.end(), 0.0);
            },
            options);
        REQUIRE(output.get_num_meshes() == 0);
        REQUIRE(output.compute_num_instances() == 0);
        REQUIRE(solved.removed_fraction == 1);
        REQUIRE(solved.worst_view_error == 0);
        REQUIRE(solved.threshold > filter.get_max_visibility_threshold());
    }
    SECTION("failed baseline retains nonempty instances and accurate statistics")
    {
        const auto [output, solved] = filter.apply_with_view_error(
            [](span<const uint8_t>, span<const uint64_t>, span<double> errors) {
                std::fill(errors.begin(), errors.end(), 0.1);
            },
            options);
        REQUIRE(output.get_num_meshes() == 3);
        REQUIRE(output.compute_num_instances() == 3);
        REQUIRE(output.get_mesh(0).get_num_facets() == 2);
        REQUIRE(output.get_mesh(1).get_num_facets() == 2);
        REQUIRE_FALSE(solved.within_error_budget);
        REQUIRE(solved.worst_view_error == 0.1);
        REQUIRE(solved.num_isolated_facets == 5);
    }
    SECTION("ordinary cuts and view-error solves share cold adjacency initialization")
    {
        OccludedFacetFilterOptions ordinary;
        ordinary.visibility_threshold = 0.1;
        ordinary.graphcut_smoothness = 0.02;
        ordinary.instancing = InstancingPolicy::FlattenInstances;
        std::promise<void> start;
        const auto gate = start.get_future().share();
        std::vector<std::future<Scene>> ordinary_results;
        std::vector<std::future<std::pair<Scene, OccludedFacetViewErrorResult>>> view_results;
        for (int i = 0; i < 3; ++i) {
            ordinary_results.push_back(std::async(std::launch::async, [&, gate] {
                gate.wait();
                return filter.apply(ordinary);
            }));
            view_results.push_back(std::async(std::launch::async, [&, gate] {
                gate.wait();
                return filter.apply_with_view_error(
                    [](span<const uint8_t> keep, span<const uint64_t>, span<double> errors) {
                        std::fill(errors.begin(), errors.end(), keep[4] ? 0.005 : 1.0);
                    },
                    options);
            }));
        }
        start.set_value();
        for (auto& future : ordinary_results) REQUIRE(future.get().get_num_meshes() == 3);
        for (auto& future : view_results) check_partial(future.get().first);
    }
    SECTION("one-shot wrapper is equivalent and validates before tracing")
    {
        RemoveOccludedFacetsOptions once;
        once.sampler_options = sampling;
        once.instancing = InstancingPolicy::Max;
        once.graphcut_smoothness = -1;
        const auto [output, solved] = remove_occluded_facets_with_view_error<Scalar, Index>(
            source,
            evaluate,
            once,
            options,
            progress,
            [](Index, Index) { return false; });
        check_partial(output);
        REQUIRE(solved.removed_fraction == Catch::Approx(0.4));
        options.num_refinement_iterations = 0;
        LA_REQUIRE_THROWS(
            remove_occluded_facets_with_view_error<Scalar, Index>(
                source,
                evaluate,
                once,
                options,
                progress,
                [&](Index, Index) {
                    ++occluder_calls;
                    return false;
                }));
    }
    check_source();
}

TEST_CASE(
    "view-error output uses the same global graph as its evaluator",
    "[raycasting][view_error]")
{
    using Scalar = double;
    using Index = uint32_t;
    using namespace lagrange;
    using namespace lagrange::raycasting;
    SurfaceMesh<Scalar, Index> mesh;
    mesh.add_vertex({0, 0, 0});
    mesh.add_vertex({1, 0, 0});
    mesh.add_vertex({0, 1, 0});
    mesh.add_vertex({0, -2, 0});
    mesh.add_triangle(0, 1, 2);
    mesh.add_triangle(1, 0, 3);
    const std::vector<Index> markers = {10, 20};
    mesh.create_attribute<Index>(
        "marker",
        AttributeElement::Facet,
        1,
        AttributeUsage::Scalar,
        markers);
    scene::SimpleScene<Scalar, Index, 3> source;
    source.add_mesh(std::move(mesh));
    source.add_instance({0});
    source.add_instance({0});
    source.ref_instance(0, 1).transform.translation().x() = 10;
    OccludedFacetSamplerOptions sampling;
    sampling.adaptive = std::nullopt;
    sampling.size_influence = 1;
    sampling.num_rays = 200;
    sampling.batch_size = 200;
    ProgressCallback progress;
    const OccludedFacetsFilter<Scalar, Index> filter(source, sampling, progress, [](Index, Index) {
        return false;
    });
    const auto measured =
        filter.estimate_visibility("visibility", InstancingPolicy::FlattenInstances);
    std::vector<double> scores;
    for (Index mi = 0; mi < 2; ++mi) {
        const auto values = measured.get_mesh(mi).get_attribute<Scalar>("visibility").get_all();
        scores.insert(scores.end(), values.begin(), values.end());
    }
    const std::vector<std::pair<uint64_t, uint64_t>> edges = {{0, 1}, {2, 3}};
    OccludedFacetViewErrorOptions options;
    options.num_threshold_iterations = 14;
    options.smoothness_tradeoff = 1;
    options.graphcut_smoothness_candidates = {1};
    std::vector<std::vector<uint8_t>> accepted;
    const auto [output, solved] = filter.apply_with_view_error(
        [&](span<const uint8_t> keep, span<const uint64_t> views, span<double> errors) {
            const bool valid = keep[1] && keep[3];
            if (valid && views.empty()) accepted.emplace_back(keep.begin(), keep.end());
            std::fill(errors.begin(), errors.end(), valid ? 0.002 : 1.0);
        },
        options);
    const auto keep =
        compute_graphcut_mask(scores, solved.threshold, edges, solved.graphcut_smoothness);
    REQUIRE(std::find(accepted.begin(), accepted.end(), keep) != accepted.end());
    REQUIRE(solved.graphcut_smoothness == 1);
    REQUIRE(solved.worst_view_error == 0.002);
    REQUIRE(solved.num_isolated_facets == 0);
    REQUIRE(output.get_num_meshes() == 2);
    for (Index mi = 0; mi < 2; ++mi) {
        const auto retained = output.get_mesh(mi).get_attribute<Index>("marker").get_all();
        std::vector<Index> expected;
        for (Index f = 0; f < 2; ++f)
            if (keep[2 * mi + f]) expected.push_back(markers[f]);
        REQUIRE(std::vector<Index>(retained.begin(), retained.end()) == expected);
    }
    REQUIRE_FALSE(source.get_mesh(0).has_edges());
    REQUIRE(source.get_mesh(0).get_num_facets() == 2);
}

TEST_CASE(
    "view-error filtering respects caller external attribute policies",
    "[raycasting][view_error]")
{
    using Scalar = float;
    using Index = uint32_t;
    using namespace lagrange;
    using namespace lagrange::raycasting;
    for (auto policy :
         {AttributeCopyPolicy::CopyIfExternal, AttributeCopyPolicy::ErrorIfExternal}) {
        std::vector<Scalar> vertices = {0, 0, 0, 1, 0, 0, 0, 1, 0, 10, 0, 0, 12, 0, 0, 10, 1, 0};
        std::vector<Index> facets = {0, 1, 2, 3, 4, 5};
        std::vector<Scalar> marker = {10, 20};
        const auto original_vertices = vertices;
        const auto original_facets = facets;
        SurfaceMesh<Scalar, Index> mesh;
        mesh.wrap_as_vertices(vertices, 6);
        mesh.wrap_as_facets(facets, 2, 3);
        mesh.wrap_as_attribute<Scalar>(
            "marker",
            AttributeElement::Facet,
            AttributeUsage::Scalar,
            1,
            marker);
        mesh.ref_vertex_to_position().set_copy_policy(AttributeCopyPolicy::CopyIfExternal);
        mesh.ref_corner_to_vertex().set_copy_policy(AttributeCopyPolicy::CopyIfExternal);
        mesh.ref_attribute<Scalar>("marker").set_copy_policy(policy);
        scene::SimpleScene<Scalar, Index, 3> source;
        source.add_mesh(std::move(mesh));
        source.add_instance({0});
        OccludedFacetSamplerOptions sampling;
        sampling.adaptive = std::nullopt;
        sampling.size_influence = 1;
        sampling.max_visibility_threshold = 0.2;
        sampling.num_rays = 200;
        sampling.batch_size = 200;
        ProgressCallback progress;
        const OccludedFacetsFilter<Scalar, Index> filter(
            source,
            sampling,
            progress,
            [](Index, Index) { return false; });
        OccludedFacetViewErrorOptions options;
        options.num_threshold_iterations = 14;
        options.smoothness_tradeoff = std::nullopt;
        auto evaluate = [](span<const uint8_t> keep, span<const uint64_t>, span<double> errors) {
            std::fill(errors.begin(), errors.end(), keep[1] ? 0.005 : 1.0);
        };
        if (policy == AttributeCopyPolicy::ErrorIfExternal) {
            LA_REQUIRE_THROWS(filter.apply_with_view_error(evaluate, options));
        } else {
            const auto [output, solved] = filter.apply_with_view_error(evaluate, options);
            REQUIRE(output.get_mesh(0).get_num_facets() == 1);
            REQUIRE(output.get_mesh(0).get_attribute<Scalar>("marker").get_all()[0] == 20);
            REQUIRE(solved.removed_fraction == 0.5);
        }
        REQUIRE(vertices == original_vertices);
        REQUIRE(facets == original_facets);
        REQUIRE(marker == std::vector<Scalar>{10, 20});
        REQUIRE(source.get_mesh(0).get_attribute<Scalar>("marker").is_external());
        REQUIRE(source.get_mesh(0).get_attribute<Scalar>("marker").get_copy_policy() == policy);
        REQUIRE(source.get_mesh(0).get_num_facets() == 2);
        REQUIRE_FALSE(source.get_mesh(0).has_edges());
    }
}
