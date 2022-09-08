/*
 * Copyright 2021 Adobe. All rights reserved.
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

#include <lagrange/create_mesh.h>

#include <openvdb/tools/VolumeToSpheres.h>
#include <openvdb/openvdb.h>

namespace lagrange {
namespace volume {

///
/// Fill a solid volume with spheres of varying radii.
///
/// @param[in]  grid         Input volume.
/// @param[out] spheres      N x 4 array of sphere centers + radii.
/// @param[in]  max_spheres  Maximum number of sphere.
/// @param[in]  overlapping  Whether to allow overlapping spheres or not.
///
/// @tparam     GridType     OpenVDB grid type.
/// @tparam     Derived      Matrix type to store sphere data.
///
template <typename GridType, typename Derived>
void fill_with_spheres(
    const GridType &grid,
    Eigen::PlainObjectBase<Derived> &spheres,
    int max_spheres,
    bool overlapping = false)
{
    openvdb::initialize();

    using Scalar = typename Derived::Scalar;
    using RowVector4s = Eigen::Matrix<float, 1, 4>;

    if (max_spheres <= 0) {
        logger().warn("Max spheres needs to be >= 1.");
        max_spheres = 1;
    }

    const openvdb::Vec2i sphere_count(1, max_spheres);

    std::vector<openvdb::Vec4s> points;
    openvdb::tools::fillWithSpheres(grid, points, sphere_count, overlapping);

    spheres.resize(points.size(), 4);

    for (size_t i = 0; i < points.size(); ++i) {
        const RowVector4s p(points[i].x(), points[i].y(), points[i].z(), points[i].w());
        spheres.row(i) << p.template cast<Scalar>();
    }
}

} // namespace volume
} // namespace lagrange
