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

class Frustum;

class AABB : public Eigen::AlignedBox3f {
public:
    using Super = Eigen::AlignedBox3f;
    using Super::Super;

    AABB() = default;
    AABB(Eigen::AlignedBox3f box) : Super(std::move(box)) { }

    /// Normalizes point to [0.0f,1.0f]^3 in AABB's bounds
    template <typename Derived>
    Eigen::Vector3f normalize_point(const Eigen::MatrixBase<Derived> &pt) const
    {
        const auto d = diagonal();
        return Eigen::Vector3f((float(pt.x()) - min().x()) / d.x(),
            (float(pt.y()) - min().y()) / d.y(),
            (float(pt.z()) - min().z()) / d.z());
    }

    /// Overload for 2D mesh
    template <typename Scalar>
    Eigen::Vector3f normalize_point(const Eigen::Matrix<Scalar, 2, 1> &pt) const
    {
        const auto d = diagonal();
        return Eigen::Vector3f(
            (float(pt.x()) - min().x()) / d.x(), (float(pt.y()) - min().y()) / d.y(), 0);
    }

    Eigen::Affine3f get_cube_transform() const;

    AABB transformed(const Eigen::Affine3f &transform) const;

    bool intersects_ray(Eigen::Vector3f origin,
        Eigen::Vector3f dir,
        float *tmin_out = nullptr,
        float *tmax_out = nullptr) const;

    bool intersects_frustum(const Frustum &f) const;
};

} // namespace ui
} // namespace lagrange
