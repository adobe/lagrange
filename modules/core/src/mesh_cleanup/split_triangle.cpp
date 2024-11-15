/*
 * Copyright 2024 Adobe. All rights reserved.
 * This file is licensed to you under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License. You may obtain a copy
 * of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under
 * the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
 * OF ANY KIND, either express or implied. See the License for the specific language
 * governing permissions and limitations under the License.
 */
#include <lagrange/SurfaceMeshTypes.h>
#include <lagrange/utils/assert.h>
#include <lagrange/utils/invalid.h>
#include <lagrange/utils/span.h>

#include "split_triangle.h"

#include <Eigen/Core>

#include <algorithm>
#include <vector>

namespace lagrange {

template <typename Scalar, typename Index>
void split_triangle(
    size_t num_points,
    span<const Scalar> points,
    span<const Index> chain,
    span<Index> visited_buffer,
    std::vector<Index>& queue_buffer,
    const Index v0,
    const Index v1,
    const Index v2,
    span<Index> triangulation)
{
    la_runtime_assert(points.size() % num_points == 0);
    la_runtime_assert(triangulation.size() % 3 == 0);
    la_runtime_assert(
        triangulation.size() / 3 + 2 == chain.size(),
        "Inconsistent triangulation size vs chian size.");
    la_runtime_assert(visited_buffer.size() == chain.size() * 3);
    Eigen::Map<const Eigen::Matrix<Scalar, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>>
        vertices(points.data(), num_points, points.size() / num_points);

    const Index chain_size = static_cast<Index>(chain.size());
    auto next = [&chain_size](Index i) { return (i + 1) % chain_size; };
    auto prev = [&chain_size](Index i) { return (i + chain_size - 1) % chain_size; };
    auto sq_length = [&](Index vi, Index vj) {
        return (vertices.row(chain[vi]) - vertices.row(chain[vj])).squaredNorm();
    };
    auto is_corner = [v0, v1, v2](Index v) { return v == v0 || v == v1 || v == v2; };

    constexpr Scalar NOT_USED = invalid<Scalar>();
    Eigen::Matrix<Index, 3, 6, Eigen::RowMajor> candidates;
    // clang-format off
    candidates <<
        v0, next(v0), prev(v0), 0, 0, 0,
        v1, next(v1), prev(v1), 0, 0, 0,
        v2, next(v2), prev(v2), 0, 0, 0;
    // clang-format on

    Eigen::Matrix<Scalar, 3, 2, Eigen::RowMajor> candidate_lengths;
    // clang-format off
    candidate_lengths <<
        sq_length(candidates(0, 1), candidates(0, 2)), NOT_USED,
        sq_length(candidates(1, 1), candidates(1, 2)), NOT_USED,
        sq_length(candidates(2, 1), candidates(2, 2)), NOT_USED;
    // clang-format on

    auto index_comp = [&candidate_lengths](Index i, Index j) {
        // Use greater than operator so the heap is min heap.
        return candidate_lengths.row(i).minCoeff() > candidate_lengths.row(j).minCoeff();
    };
    auto& Q = queue_buffer;
    Q.clear();
    Q.push_back(0);
    Q.push_back(1);
    Q.push_back(2);
    std::make_heap(Q.begin(), Q.end(), index_comp);

    Eigen::Map<Eigen::Matrix<Index, Eigen::Dynamic, 3, Eigen::RowMajor>> visited(
        visited_buffer.data(),
        chain_size,
        3);
    visited.setZero();
    visited(v0, 0) = 1;
    visited(v1, 1) = 1;
    visited(v2, 2) = 1;

    size_t num_generated_triangles = 0;
    while (!Q.empty()) {
        Index idx = Q.front();
        std::pop_heap(Q.begin(), Q.end(), index_comp);
        Q.pop_back();
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
        la_runtime_assert(candidate_lengths.row(idx).minCoeff() <= candidate_lengths.minCoeff());

        Index base_v = candidates(idx, selection * 3);
        Index right_v = candidates(idx, selection * 3 + 1);
        Index left_v = candidates(idx, selection * 3 + 2);
        la_runtime_assert(base_v >= 0 && base_v < chain_size);
        la_runtime_assert(right_v >= 0 && right_v < chain_size);
        la_runtime_assert(left_v >= 0 && left_v < chain_size);
        la_runtime_assert(visited(base_v, idx) >= 1);

        // A special case.
        if (is_corner(base_v) && is_corner(right_v) && is_corner(left_v)) {
            if (chain_size != 3) {
                candidate_lengths(idx, selection) = NOT_USED;
                Q.push_back(idx);
                std::push_heap(Q.begin(), Q.end(), index_comp);
                continue;
            }
        }

        if (visited.row(base_v).sum() > 1 || visited(right_v, idx) > 1 ||
            visited(left_v, idx) > 1) {
            // This canadidate is invalid.
            candidate_lengths(idx, selection) = NOT_USED;
            Q.push_back(idx);
            std::push_heap(Q.begin(), Q.end(), index_comp);
            continue;
        }

        visited(base_v, idx) = 2;
        visited(right_v, idx) = 1;
        visited(left_v, idx) = 1;
        triangulation[num_generated_triangles * 3] = chain[base_v];
        triangulation[num_generated_triangles * 3 + 1] = chain[right_v];
        triangulation[num_generated_triangles * 3 + 2] = chain[left_v];
        num_generated_triangles++;

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
        Q.push_back(idx);
        std::push_heap(Q.begin(), Q.end(), index_comp);
    }

    Eigen::Matrix<Eigen::Index, Eigen::Dynamic, 1> visited_sum =
        (visited.array() > 0).rowwise().count();
    if ((visited_sum.array() > 1).count() == 3) {
        Index f[3];
        Index count = 0;
        for (Index i = 0; i < chain_size; i++) {
            if (visited_sum[i] > 1) {
                la_runtime_assert(count < 3);
                f[count] = chain[i];
                count++;
            }
        }
        la_runtime_assert(count == 3);
        triangulation[num_generated_triangles * 3] = f[0];
        triangulation[num_generated_triangles * 3 + 1] = f[1];
        triangulation[num_generated_triangles * 3 + 2] = f[2];
        num_generated_triangles++;
    }
    la_debug_assert(num_generated_triangles + 2 == chain.size());
}

#define LA_X_split_triangle(_, Scalar, Index)                \
    template LA_CORE_API void split_triangle<Scalar, Index>( \
        size_t,                                              \
        span<const Scalar>,                                  \
        span<const Index>,                                   \
        span<Index>,                                         \
        std::vector<Index>&,                                 \
        const Index,                                         \
        const Index,                                         \
        const Index,                                         \
        span<Index>);
LA_SURFACE_MESH_X(split_triangle, 0)

} // namespace lagrange
