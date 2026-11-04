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

#include "solve_graphcut_with_view_error.h"

#include "compute_graphcut_mask.h"

#include <lagrange/Logger.h>
#include <lagrange/utils/assert.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <numeric>
#include <set>
#include <vector>

namespace lagrange::raycasting {

namespace {

/// Highest error across a per-view vector; 0 for an empty set.
double worst_of(span<const double> per_view)
{
    return per_view.empty() ? 0.0 : *std::max_element(per_view.begin(), per_view.end());
}

/// Indices of the up-to-k highest-error views, most-offending first.
std::vector<uint64_t> rank_worst_views(const std::vector<double>& per_view, uint64_t k)
{
    std::vector<uint64_t> idx(per_view.size());
    std::iota(idx.begin(), idx.end(), uint64_t{0});
    std::stable_sort(idx.begin(), idx.end(), [&](uint64_t a, uint64_t b) {
        return per_view[a] > per_view[b];
    });
    if (idx.size() > k) idx.resize(k);
    return idx;
}

} // namespace

void validate_view_error_options(const OccludedFacetViewErrorOptions& options)
{
    la_runtime_assert(
        std::isfinite(options.max_view_error) && options.max_view_error >= 0,
        "max_view_error must be finite and non-negative");
    la_runtime_assert(
        options.num_threshold_iterations > 0,
        "num_threshold_iterations must be positive");
    la_runtime_assert(
        options.num_refinement_iterations > 0,
        "num_refinement_iterations must be positive");
    la_runtime_assert(options.num_views > 0, "num_views must be positive");
    if (options.smoothness_tradeoff) {
        la_runtime_assert(
            std::isfinite(*options.smoothness_tradeoff) && *options.smoothness_tradeoff >= 0 &&
                *options.smoothness_tradeoff <= 1,
            "smoothness_tradeoff must be finite and in [0, 1]");
    }
    for (double weight : options.graphcut_smoothness_candidates) {
        la_runtime_assert(
            std::isfinite(weight) && weight > 0,
            "graphcut_smoothness_candidates must be finite and positive");
    }
}

OccludedFacetViewErrorResult solve_graphcut_with_view_error(
    span<const double> scores,
    span<const std::pair<uint64_t, uint64_t>> adjacency_edges,
    RenderErrorEvaluator evaluate,
    const OccludedFacetViewErrorOptions& options,
    uint64_t num_worst_views)
{
    validate_view_error_options(options);
    la_runtime_assert(num_worst_views > 0, "num_worst_views must be positive");
    const uint64_t n = scores.size();

    double signal_max = 0;
    for (double score : scores) signal_max = std::max(signal_max, score);

    auto build_mask = [&](double threshold, double smoothness) {
        return compute_graphcut_mask(scores, threshold, adjacency_edges, smoothness);
    };
    auto num_removed = [](const std::vector<uint8_t>& mask) {
        return static_cast<uint64_t>(std::count(mask.begin(), mask.end(), uint8_t{0}));
    };
    auto removed_fraction = [&](uint64_t removed) {
        return n > 0 ? static_cast<double>(removed) / static_cast<double>(n) : 0.0;
    };
    auto count_floaters = [&](const std::vector<uint8_t>& mask) {
        std::vector<uint8_t> has_kept_neighbor(n, 0);
        for (const auto& [first, second] : adjacency_edges) {
            if (mask[first] && mask[second]) {
                has_kept_neighbor[first] = 1;
                has_kept_neighbor[second] = 1;
            }
        }
        uint64_t floaters = 0;
        for (uint64_t i = 0; i < n; ++i) {
            if (mask[i] && !has_kept_neighbor[i]) ++floaters;
        }
        return floaters;
    };
    auto eval = [&](const std::vector<uint8_t>& mask,
                    span<const uint64_t> views,
                    std::vector<double>& errors) {
        const size_t num_errors = views.empty() ? options.num_views : views.size();
        errors.resize(num_errors);
        evaluate(
            span<const uint8_t>(mask.data(), mask.size()),
            views,
            span<double>(errors.data(), errors.size()));
        for (double error : errors) {
            la_runtime_assert(
                std::isfinite(error) && error >= 0,
                "evaluator returned invalid error");
        }
    };
    const span<const uint64_t> all_views{};

    // Validate graph inputs even when the all-kept baseline fails.
    const auto all_kept = build_mask(0.0, 0.0);
    std::vector<double> all_kept_errors;
    eval(all_kept, all_views, all_kept_errors);
    OccludedFacetViewErrorResult best;
    best.num_isolated_facets = count_floaters(all_kept);
    best.worst_view_error = worst_of(all_kept_errors);
    best.within_error_budget = best.worst_view_error <= options.max_view_error;
    if (!best.within_error_budget) {
        lagrange::logger().warn("Graphcut error solve: all-kept mask exceeds budget");
        return best;
    }

    auto lambdas = options.graphcut_smoothness_candidates;
    if (lambdas.empty()) {
        constexpr std::array<double, 10> factors =
            {1e-4, 5e-4, 1e-3, 5e-3, 1e-2, 5e-2, 0.1, 0.15, 0.2, 0.5};
        lambdas.reserve(factors.size());
        for (double factor : factors) lambdas.push_back(factor * signal_max);
    } else {
        std::sort(lambdas.begin(), lambdas.end());
        lambdas.erase(std::unique(lambdas.begin(), lambdas.end()), lambdas.end());
    }

    // Phase 1 searches the threshold against every view, without smoothing.
    lagrange::logger().info(
        "Graphcut error solve: phase 1 (threshold only), budget {:.6f}",
        options.max_view_error);
    double candidate = 0.0;
    const double threshold_max =
        signal_max > 0 ? std::nextafter(signal_max, std::numeric_limits<double>::max()) : 1.0;
    double step = threshold_max / 2.0;
    double p1_thresh = 0.0;
    double p1_frac = 0.0;
    std::vector<double> p1_errors = all_kept_errors;
    bool have_removal = false;
    std::vector<double> probe_errors;
    for (uint64_t i = 0; i < options.num_threshold_iterations; ++i) {
        const double probe = candidate + step;
        const auto mask = build_mask(probe, 0.0);
        const double fr = removed_fraction(num_removed(mask));
        eval(mask, all_views, probe_errors);
        const double error = worst_of(probe_errors);
        lagrange::logger().info(
            "  phase 1 iter {}/{}: threshold {:.6f}, error {:.6f}, removed_fraction {:.4f}",
            i + 1,
            options.num_threshold_iterations,
            probe,
            error,
            fr);
        if (error <= options.max_view_error) {
            candidate = probe;
            if (fr > 0) {
                p1_thresh = probe;
                p1_frac = fr;
                p1_errors = probe_errors;
                have_removal = true;
            }
        }
        step /= 2.0;
    }

    // Evaluate the finite endpoint; DBL_MAX scores cannot be culled by a finite threshold.
    const auto endpoint_mask = build_mask(threshold_max, 0.0);
    const double endpoint_frac = removed_fraction(num_removed(endpoint_mask));
    eval(endpoint_mask, all_views, probe_errors);
    const double endpoint_error = worst_of(probe_errors);
    lagrange::logger().info(
        "  phase 1 endpoint: threshold {:.6f}, error {:.6f}, removed_fraction {:.4f}",
        threshold_max,
        endpoint_error,
        endpoint_frac);
    if (endpoint_error <= options.max_view_error && endpoint_frac > 0) {
        p1_thresh = threshold_max;
        p1_frac = endpoint_frac;
        p1_errors = probe_errors;
        have_removal = true;
    }
    if (!have_removal) {
        lagrange::logger().info("Graphcut error solve: no removal stays within budget");
        return best;
    }

    const auto p1_mask = build_mask(p1_thresh, 0.0);
    const uint64_t p1_removed = num_removed(p1_mask);
    const uint64_t p1_floaters = count_floaters(p1_mask);
    best.threshold = p1_thresh;
    best.removed_fraction = p1_frac;
    best.num_isolated_facets = p1_floaters;
    best.worst_view_error = worst_of(p1_errors);
    lagrange::logger().info(
        "Phase 1: threshold {:.6f}, removed_fraction {:.4f}, isolated_facets {}",
        p1_thresh,
        p1_frac,
        p1_floaters);
    if (!options.smoothness_tradeoff) {
        lagrange::logger().info("Skipping phase 2 (smoothness_tradeoff disabled)");
        return best;
    }

    // Compare lost facet counts rather than subtracting rounded removal fractions.
    const double max_removed_loss = *options.smoothness_tradeoff * static_cast<double>(p1_removed);
    auto exceeds_removal_loss = [&](uint64_t removed) {
        return removed<p1_removed&& static_cast<double>(p1_removed - removed)> max_removed_loss;
    };
    auto worst_views = rank_worst_views(p1_errors, num_worst_views);
    uint64_t prev_floaters = p1_floaters;
    for (double lam : lambdas) {
        double lam_candidate = 0.0;
        double lam_step = signal_max / 2.0;
        double lam_best_thresh = 0.0;
        double lam_best_frac = 0.0;
        uint64_t lam_best_removed = 0;
        bool lam_ok = false;
        for (uint64_t i = 0; i < options.num_threshold_iterations; ++i) {
            const double probe = lam_candidate + lam_step;
            const auto mask = build_mask(probe, lam);
            const uint64_t removed = num_removed(mask);
            const double fr = removed_fraction(removed);
            eval(mask, {worst_views.data(), worst_views.size()}, probe_errors);
            const double error = worst_of(probe_errors);
            lagrange::logger().info(
                "  lambda {:.5g} iter {}/{}: threshold {:.6f}, worst_error {:.6f}, "
                "removed_fraction {:.4f}",
                lam,
                i + 1,
                options.num_threshold_iterations,
                probe,
                error,
                fr);
            if (error <= options.max_view_error) {
                lam_candidate = probe;
                lam_best_thresh = probe;
                lam_best_frac = fr;
                lam_best_removed = removed;
                lam_ok = true;
            }
            lam_step /= 2.0;
        }
        if (!lam_ok) break;
        if (exceeds_removal_loss(lam_best_removed)) break;

        // Verify on every view, adding a newly-offending view and refining toward threshold 0.
        bool verified = false;
        std::vector<double> full_per_view;
        for (int attempt = 0; attempt < 3; ++attempt) {
            const auto mask = build_mask(lam_best_thresh, lam);
            eval(mask, all_views, full_per_view);
            if (worst_of(full_per_view) <= options.max_view_error) {
                verified = true;
                break;
            }
            const uint64_t new_worst = static_cast<uint64_t>(
                std::max_element(full_per_view.begin(), full_per_view.end()) -
                full_per_view.begin());
            if (std::find(worst_views.begin(), worst_views.end(), new_worst) != worst_views.end())
                break;
            worst_views.push_back(new_worst);
            lagrange::logger().info(
                "  lambda {:.5g}: verify failed, view {} over budget (error {:.6f}), refining",
                lam,
                new_worst,
                worst_of(full_per_view));

            double r_candidate = 0.0;
            double r_step = lam_candidate / 2.0;
            lam_best_thresh = 0.0;
            lam_best_frac = 0.0;
            lam_best_removed = 0;
            for (uint64_t i = 0; i < options.num_refinement_iterations; ++i) {
                const double probe = r_candidate + r_step;
                const auto refinement_mask = build_mask(probe, lam);
                const uint64_t removed = num_removed(refinement_mask);
                const double fr = removed_fraction(removed);
                eval(refinement_mask, {worst_views.data(), worst_views.size()}, probe_errors);
                const double error = worst_of(probe_errors);
                lagrange::logger().info(
                    "  lambda {:.5g} refine {}/{}: threshold {:.6f}, worst_error {:.6f}, "
                    "removed_fraction {:.4f}",
                    lam,
                    i + 1,
                    options.num_refinement_iterations,
                    probe,
                    error,
                    fr);
                if (error <= options.max_view_error) {
                    r_candidate = probe;
                    lam_best_thresh = probe;
                    lam_best_frac = fr;
                    lam_best_removed = removed;
                }
                r_step /= 2.0;
            }
            lam_candidate = r_candidate;
            if (exceeds_removal_loss(lam_best_removed)) break;
        }
        if (!verified) break;
        const uint64_t lam_floaters = count_floaters(build_mask(lam_best_thresh, lam));
        if (lam_floaters >= prev_floaters) break;

        best.graphcut_smoothness = lam;
        best.threshold = lam_best_thresh;
        best.removed_fraction = lam_best_frac;
        best.num_isolated_facets = lam_floaters;
        best.worst_view_error = worst_of(full_per_view);
        prev_floaters = lam_floaters;
        const auto top = rank_worst_views(full_per_view, num_worst_views);
        std::set<uint64_t> merged(worst_views.begin(), worst_views.end());
        merged.insert(top.begin(), top.end());
        worst_views.assign(merged.begin(), merged.end());
        lagrange::logger().info(
            "lambda {:.5g}: threshold {:.6f}, removed_fraction {:.4f}, isolated_facets {}",
            lam,
            lam_best_thresh,
            lam_best_frac,
            lam_floaters);
    }
    lagrange::logger().info(
        "Graphcut error solve: chosen smoothness {:.5g}, threshold {:.6f}, removed_fraction {:.4f}",
        best.graphcut_smoothness,
        best.threshold,
        best.removed_fraction);
    return best;
}

} // namespace lagrange::raycasting
