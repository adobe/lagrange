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

#include "compute_graphcut_mask.h"

#include <lagrange/utils/assert.h>

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <fcbk/fcbk.hpp>
#include <lagrange/utils/warnon.h>
// clang-format on

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace lagrange::raycasting {

namespace {

using Graph = fcbk::Graph<uint32_t, true>;
using Capacity = int32_t;

// fcBK requires initial residual capacities to be strictly below 2^30 - 1.
constexpr Capacity max_scaled_capacity = (Capacity{1} << 30) - 2;

Capacity scale_capacity(double value, double max_capacity)
{
    const auto scaled =
        std::llround(value / max_capacity * static_cast<double>(max_scaled_capacity));
    return static_cast<Capacity>(
        std::clamp(scaled, 0LL, static_cast<long long>(max_scaled_capacity)));
}

} // namespace

std::vector<uint8_t> compute_graphcut_mask(
    span<const double> scores,
    double threshold,
    span<const std::pair<uint64_t, uint64_t>> adjacency_edges,
    double smoothness)
{
    la_runtime_assert(
        std::isfinite(threshold) && threshold >= 0,
        "threshold must be finite and non-negative");
    la_runtime_assert(
        std::isfinite(smoothness) && smoothness >= 0,
        "smoothness must be finite and non-negative");

    for (double score : scores) {
        la_runtime_assert(
            std::isfinite(score) && score >= 0,
            "scores must be finite and non-negative");
    }
    for (const auto& [first, second] : adjacency_edges) {
        la_runtime_assert(
            first < scores.size() && second < scores.size(),
            "adjacency edge out of range");
        la_runtime_assert(first != second, "adjacency edge must connect distinct facets");
    }

    std::vector<uint8_t> keep(scores.size());
    if (smoothness == 0) {
        for (size_t i = 0; i < scores.size(); ++i) {
            keep[i] = scores[i] >= threshold ? uint8_t{1} : uint8_t{0};
        }
        return keep;
    }

    const size_t num_facets = scores.size();
    la_runtime_assert(
        num_facets <= std::numeric_limits<uint32_t>::max(),
        "too many facets for fcBK");

    double max_capacity = std::max(threshold, smoothness);
    for (double score : scores) max_capacity = std::max(max_capacity, score);
    Graph graph(num_facets);

    for (size_t i = 0; i < num_facets; ++i) {
        graph.add_tweights(
            static_cast<uint32_t>(i),
            scale_capacity(scores[i], max_capacity),
            scale_capacity(threshold, max_capacity));
    }
    for (const auto& [first, second] : adjacency_edges) {
        graph.add_edge(
            static_cast<uint32_t>(first),
            static_cast<uint32_t>(second),
            scale_capacity(smoothness, max_capacity),
            scale_capacity(smoothness, max_capacity));
    }

    graph.init_maxflow();
    graph.maxflow();
    const auto partition = graph.get_partition();
    for (size_t i = 0; i < num_facets; ++i) {
        keep[i] = partition[i] == fcbk::SOURCE ? uint8_t{1} : uint8_t{0};
    }
    return keep;
}

} // namespace lagrange::raycasting
