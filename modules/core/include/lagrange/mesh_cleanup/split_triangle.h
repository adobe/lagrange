/*
 * Copyright 2020 Adobe. All rights reserved.
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

#include <iostream>
#include <limits>
#include <queue>
#include <vector>

#include <lagrange/common.h>
#include <lagrange/utils/safe_cast.h>

namespace lagrange {
/**
 * Split triangle into smaller triangles based on the chain of spliting
 * points.
 *
 * Arguments:
 *     vertices: list of vertices that contains all 3 corner of the triangle
 *               and all splitting points.
 *     chain:    A chain of indices into vertices that iterates all
 *               splitting points and corners in counterclockwise order.
 *     v0: index into chain that represents corner 0.
 *     v1: index into chain that represents corner 1.
 *     v2: index into chain that represents corner 2.
 *
 * Returns:
 *     facets: output facets.
 */
template <typename VertexArray, typename Index>
std::vector<Eigen::Matrix<Index, 3, 1>> split_triangle(
    const VertexArray& vertices,
    const std::vector<Index>& chain,
    const Index v0,
    const Index v1,
    const Index v2)
{
    const Index chain_size = safe_cast<Index>(chain.size());
    auto next = [&chain_size](Index i) { return (i + 1) % chain_size; };
    auto prev = [&chain_size](Index i) { return (i + chain_size - 1) % chain_size; };
    auto sq_length = [&vertices, &chain](Index vi, Index vj) {
        return (vertices.row(chain[vi]) - vertices.row(chain[vj])).squaredNorm();
    };
    auto is_corner = [v0, v1, v2](Index v) { return v == v0 || v == v1 || v == v2; };
    std::vector<Eigen::Matrix<Index, 3, 1>> facets;

    const double NOT_USED = std::numeric_limits<double>::max();
    Eigen::Matrix<Index, 3, 6, Eigen::RowMajor> candidates;
    candidates << v0, next(v0), prev(v0), 0, 0, 0, v1, next(v1), prev(v1), 0, 0, 0, v2, next(v2),
        prev(v2), 0, 0, 0;

    Eigen::Matrix<double, 3, 2, Eigen::RowMajor> candidate_lengths;
    candidate_lengths << sq_length(candidates(0, 1), candidates(0, 2)), NOT_USED,
        sq_length(candidates(1, 1), candidates(1, 2)), NOT_USED,
        sq_length(candidates(2, 1), candidates(2, 2)), NOT_USED;

    auto index_comp = [&candidate_lengths](Index i, Index j) {
        // Use greater than operator so the heap is min heap.
        return candidate_lengths.row(i).minCoeff() > candidate_lengths.row(j).minCoeff();
    };
    std::priority_queue<Index, std::vector<Index>, decltype(index_comp)> Q(index_comp);
    Q.push(0);
    Q.push(1);
    Q.push(2);

    Eigen::Matrix<Index, Eigen::Dynamic, 3, Eigen::RowMajor> visited(chain_size, 3);
    visited.setZero();
    visited(v0, 0) = 1;
    visited(v1, 1) = 1;
    visited(v2, 2) = 1;

    while (!Q.empty()) {
        Index idx = Q.top();
        Q.pop();
        Index selection = 0;
        if (candidate_lengths(idx, 0) != NOT_USED &&
            candidate_lengths(idx, 0) <= candidate_lengths(idx, 1)) {
            selection = 0;
        } else if (
            candidate_lengths(idx, 1) != NOT_USED &&
            candidate_lengths(idx, 1) <= candidate_lengths(idx, 0)) {
            selection = 1;
        } else {
            // Select neither candidate from this corner.
            continue;
        }
        LA_ASSERT(candidate_lengths.row(idx).minCoeff() <= candidate_lengths.minCoeff());

        Index base_v = candidates(idx, selection * 3);
        Index right_v = candidates(idx, selection * 3 + 1);
        Index left_v = candidates(idx, selection * 3 + 2);
        LA_ASSERT(base_v >= 0 && base_v < chain_size);
        LA_ASSERT(right_v >= 0 && right_v < chain_size);
        LA_ASSERT(left_v >= 0 && left_v < chain_size);
        LA_ASSERT(visited(base_v, idx) >= 1);

        // A special case.
        if (is_corner(base_v) && is_corner(right_v) && is_corner(left_v)) {
            if (chain_size != 3) {
                candidate_lengths(idx, selection) = NOT_USED;
                Q.push(idx);
                continue;
            }
        }

        if (visited.row(base_v).sum() > 1 || visited(right_v, idx) > 1 ||
            visited(left_v, idx) > 1) {
            // This canadidate is invalid.
            candidate_lengths(idx, selection) = NOT_USED;
            Q.push(idx);
            continue;
        }

        visited(base_v, idx) = 2;
        visited(right_v, idx) = 1;
        visited(left_v, idx) = 1;
        facets.push_back({chain[base_v], chain[right_v], chain[left_v]});

        // Update candidate from this corner.
        if (visited.row(right_v).sum() == 1) {
            Index right_to_right = next(right_v);
            candidate_lengths(idx, 0) = sq_length(left_v, right_to_right);
            candidates(idx, 0) = right_v;
            candidates(idx, 1) = right_to_right;
            candidates(idx, 2) = left_v;
        } else {
            candidate_lengths(idx, 0) = NOT_USED;
        }

        if (visited.row(left_v).sum() == 1) {
            Index left_to_left = prev(left_v);
            candidate_lengths(idx, 1) = sq_length(right_v, left_to_left);
            candidates(idx, 3) = left_v;
            candidates(idx, 4) = right_v;
            candidates(idx, 5) = left_to_left;
        } else {
            candidate_lengths(idx, 1) = NOT_USED;
        }
        Q.push(idx);
    }

    Eigen::Matrix<Eigen::Index, Eigen::Dynamic, 1> visited_sum =
        (visited.array() > 0).rowwise().count();
    if ((visited_sum.array() > 1).count() == 3) {
        Index f[3];
        Index count = 0;
        for (Index i = 0; i < chain_size; i++) {
            if (visited_sum[i] > 1) {
                LA_ASSERT(count < 3);
                f[count] = chain[i];
                count++;
            }
        }
        LA_ASSERT(count == 3);
        facets.push_back({f[0], f[1], f[2]});
    }
    return facets;
}
} // namespace lagrange
