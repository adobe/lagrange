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

#include <lagrange/ui/utils/math.h>
#include <array>

namespace lagrange {
namespace ui {

enum FrustumPlanes : unsigned int {
    FRUSTUM_NEAR = 0,
    FRUSTUM_FAR,
    FRUSTUM_LEFT,
    FRUSTUM_RIGHT,
    FRUSTUM_TOP,
    FRUSTUM_BOTTOM
};

enum FrustumVertices : unsigned int {
    FRUSTUM_NEAR_LEFT_BOTTOM = 0,
    FRUSTUM_FAR_LEFT_BOTTOM,
    FRUSTUM_NEAR_RIGHT_BOTTOM,
    FRUSTUM_FAR_RIGHT_BOTTOM,
    FRUSTUM_NEAR_LEFT_TOP,
    FRUSTUM_FAR_LEFT_TOP,
    FRUSTUM_NEAR_RIGHT_TOP,
    FRUSTUM_FAR_RIGHT_TOP
};

/// Frustum defined using 6 planes
class Frustum
{
public:
    using Plane = Eigen::Hyperplane<float, 3>;

    std::array<Eigen::Vector3f, 8> vertices;
    std::array<Plane, 6> planes;

    Frustum transformed(const Eigen::Affine3f& T) const;

    bool intersects(const Eigen::AlignedBox3f& bb, bool& fully_inside) const;
    bool intersects(
        const Eigen::Vector3f& a, const Eigen::Vector3f& b, const Eigen::Vector3f& c) const;

    bool is_backfacing(
        const Eigen::Vector3f& a, const Eigen::Vector3f& b, const Eigen::Vector3f& c) const;

    bool intersects(const Eigen::Vector3f& a, const Eigen::Vector3f& b) const;

    Eigen::Vector3f get_edge(FrustumVertices a, FrustumVertices b) const;
    Eigen::Vector3f get_normalized_edge(FrustumVertices a, FrustumVertices b) const;


    bool contains(const Eigen::Vector3f& a) const;
};


} // namespace ui
} // namespace lagrange
