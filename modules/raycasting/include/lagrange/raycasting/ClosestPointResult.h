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

#include <lagrange/common.h>

#include <Eigen/Core>

#include <functional>

namespace lagrange {
namespace raycasting {

template <typename Scalar>
struct ClosestPointResult
{
    // Point type
    using Point = Eigen::Matrix<Scalar, 3, 1>;

    // Callback to populate triangle corner position given a (mesh_id, facet_id)
    std::function<void(unsigned, unsigned, Point&, Point&, Point&)> populate_triangle;

    // Current best result
    unsigned mesh_index = invalid<unsigned>();
    unsigned facet_index = invalid<unsigned>();
    Point closest_point;
    Point barycentric_coord;
};

} // namespace raycasting

} // namespace lagrange
