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
#pragma once

#include <lagrange/utils/span.h>

#include <vector>

namespace lagrange::internal {

///
/// Split a triangle into smaller triangles based on the chain of splitting
/// points on the edges.
///
/// @param num_points Total number of points.
/// @param points Coordinates of the points.
/// @param chain  Chain of indices into points that iterates all vertices and
///               splitting points in counterclockwise order around the triangle.
/// @param visited_buffer Temporary buffer that should be of size 3 * chain.size().
/// @param queue_buffer Temporary buffer used for priority queue.
/// @param v0 Index of the first corner of the input triangle in `chain`.
/// @param v1 Index of the second corner of the input triangle in `chain`.
/// @param v2 Index of the third corner of the input triangle in `chain`.
///
/// @param[out] triangulation The (n-2) by 3 array of output triangles,
///                           where n is the size of the chain.
///
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
    span<Index> triangulation);

} // namespace lagrange::internal
