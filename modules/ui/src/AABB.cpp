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
#include <lagrange/ui/AABB.h>
#include <lagrange/ui/Frustum.h>

namespace lagrange {
namespace ui {

AABB AABB::transformed(const Eigen::Affine3f& transform) const
{
    Eigen::AlignedBox3f new_box;
    for (int i = 0; i < 8; ++i) {
        new_box.extend(transform * corner(static_cast<Eigen::AlignedBox3f::CornerType>(i)));
    }

    return new_box;
}

Eigen::Affine3f AABB::get_cube_transform() const
{
    return Eigen::Translation3f(min()) * Eigen::Scaling(diagonal()) *
           Eigen::Translation3f(Eigen::Vector3f::Constant(0.5f)) *
           Eigen::Scaling(Eigen::Vector3f::Constant(0.5f));
}

bool AABB::intersects_ray(Eigen::Vector3f origin,
    Eigen::Vector3f dir,
    float* tmin_out /* = nullptr*/,
    float* tmax_out /* = nullptr*/) const
{
    float tmin = (min().x() - origin.x()) / dir.x();
    float tmax = (max().x() - origin.x()) / dir.x();

    if (tmin > tmax) std::swap(tmin, tmax);

    float tymin = (min().y() - origin.y()) / dir.y();
    float tymax = (max().y() - origin.y()) / dir.y();

    if (tymin > tymax) std::swap(tymin, tymax);

    if ((tmin > tymax) || (tymin > tmax)) return false;

    if (tymin > tmin) tmin = tymin;

    if (tymax < tmax) tmax = tymax;

    float tzmin = (min().z() - origin.z()) / dir.z();
    float tzmax = (max().z() - origin.z()) / dir.z();

    if (tzmin > tzmax) std::swap(tzmin, tzmax);

    if ((tmin > tzmax) || (tzmin > tmax)) return false;

    if (tzmin > tmin) tmin = tzmin;

    if (tzmax < tmax) tmax = tzmax;

    if (tmin_out) *tmin_out = tmin;
    if (tmax_out) *tmax_out = tmax;

    return true;
}

// TODO: Replace by Frustum::intersects()?
bool AABB::intersects_frustum(const Frustum& f) const
{
    for (size_t i = 0; i < f.planes.size(); ++i) {
        int out = 0;
        // clang-format off
        out += ((f.planes[i].coeffs().dot(Eigen::Vector4f(min().x(), min().y(), min().z(), 1.0f)) < 0.0f) ? 1 : 0);
        out += ((f.planes[i].coeffs().dot(Eigen::Vector4f(max().x(), min().y(), min().z(), 1.0f)) < 0.0f) ? 1 : 0);
        out += ((f.planes[i].coeffs().dot(Eigen::Vector4f(min().x(), max().y(), min().z(), 1.0f)) < 0.0f) ? 1 : 0);
        out += ((f.planes[i].coeffs().dot(Eigen::Vector4f(max().x(), max().y(), min().z(), 1.0f)) < 0.0f) ? 1 : 0);
        out += ((f.planes[i].coeffs().dot(Eigen::Vector4f(min().x(), min().y(), max().z(), 1.0f)) < 0.0f) ? 1 : 0);
        out += ((f.planes[i].coeffs().dot(Eigen::Vector4f(max().x(), min().y(), max().z(), 1.0f)) < 0.0f) ? 1 : 0);
        out += ((f.planes[i].coeffs().dot(Eigen::Vector4f(min().x(), max().y(), max().z(), 1.0f)) < 0.0f) ? 1 : 0);
        out += ((f.planes[i].coeffs().dot(Eigen::Vector4f(max().x(), max().y(), max().z(), 1.0f)) < 0.0f) ? 1 : 0);
        // clang-format on
        if (out == 8) return false;
    }
    return true;
}

} // namespace ui
} // namespace lagrange
