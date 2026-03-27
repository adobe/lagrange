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

#include "clip_triangle_by_bbox.h"

#include <lagrange/AttributeTypes.h>
#include <lagrange/utils/assert.h>

namespace lagrange::texproc::internal {

namespace {

enum class Sign {
    Negative = -1,
    Zero = 0,
    Positive = 1,
};

template <typename ScalarT, int Axis, bool Invert>
struct AlignedHalfPlane
{
    using Scalar = ScalarT;
    using RowVector2s = Eigen::Matrix<Scalar, 1, 2>;
    static constexpr int axis = Axis;
    static constexpr bool invert = Invert;
    Scalar coord = 0;
};

template <typename HalfPlaneType>
inline Sign point_is_in_aligned_half_plane(
    const typename HalfPlaneType::RowVector2s& p,
    HalfPlaneType half_plane)
{
    if (p[half_plane.axis] == half_plane.coord) {
        return Sign::Zero;
    } else if (p[half_plane.axis] > half_plane.coord) {
        return half_plane.invert ? Sign::Negative : Sign::Positive;
    } else {
        return half_plane.invert ? Sign::Positive : Sign::Negative;
    }
}

template <typename HalfPlaneType>
inline bool intersect_line_half_plane(
    const typename HalfPlaneType::RowVector2s& p1,
    const typename HalfPlaneType::RowVector2s& p2,
    HalfPlaneType half_plane,
    typename HalfPlaneType::RowVector2s& result)
{
    constexpr int axis = half_plane.axis;

    if (p1[axis] == p2[axis]) {
        return false;
    }

    using Scalar = typename HalfPlaneType::Scalar;

    // Sort endpoints by the clipped axis to ensure the interpolation produces
    // bit-identical results regardless of which side of the half-plane is being clipped.
    auto [lo, hi] =
        std::minmax(p1, p2, [&](const auto& a, const auto& b) { return a[axis] < b[axis]; });
    const Scalar t = (half_plane.coord - lo[axis]) / (hi[axis] - lo[axis]);
    result = (Scalar(1) - t) * lo + t * hi;

    // Snap the axis coordinate to the exact half-plane value.
    result[axis] = half_plane.coord;

    return true;
}

template <typename HalfPlaneType>
SmallPolygon2<typename HalfPlaneType::Scalar, 7> clip_small_poly_by_aligned_half_plane(
    const SmallPolygon2<typename HalfPlaneType::Scalar, 7>& poly,
    HalfPlaneType half_plane)
{
    using Scalar = typename HalfPlaneType::Scalar;
    using RowVector2s = typename HalfPlaneType::RowVector2s;

    SmallPolygon2<Scalar, 7> result(0, 2);

    auto push_back = [&](const RowVector2s& p) {
        la_debug_assert(result.rows() != 7);
        int idx = static_cast<int>(result.rows());
        result.conservativeResize(idx + 1, Eigen::NoChange);
        result.row(idx) = p;
    };

    if (poly.rows() == 0) {
        return result;
    }

    if (poly.rows() == 1) {
        if (point_is_in_aligned_half_plane(poly.row(0), half_plane) != Sign::Zero) {
            push_back(poly.row(0));
        }
        return result;
    }

    RowVector2s prev_p = poly.row(poly.rows() - 1);
    Sign prev_status = point_is_in_aligned_half_plane(prev_p, half_plane);

    for (Eigen::Index i = 0; i < poly.rows(); ++i) {
        const RowVector2s p = poly.row(i);
        const Sign status = point_is_in_aligned_half_plane(p, half_plane);
        if (status != prev_status && status != Sign::Zero && prev_status != Sign::Zero) {
            RowVector2s intersect;
            if (intersect_line_half_plane<HalfPlaneType>(prev_p, p, half_plane, intersect)) {
                push_back(intersect);
            }
        }

        switch (status) {
        case Sign::Negative: break;
        case Sign::Zero: [[fallthrough]];
        case Sign::Positive: push_back(p); break;
        default: break;
        }

        prev_p = p;
        prev_status = status;
    }

    return result;
}

} // anonymous namespace

// -----------------------------------------------------------------------------

template <typename Scalar>
SmallPolygon2<Scalar, 7> clip_triangle_by_bbox(
    const SmallPolygon2<Scalar, 3>& triangle,
    const Eigen::AlignedBox<Scalar, 2>& bbox)
{
    const AlignedHalfPlane<Scalar, 0, false> h0{bbox.min().x()};
    const AlignedHalfPlane<Scalar, 0, true> h1{bbox.max().x()};
    const AlignedHalfPlane<Scalar, 1, false> h2{bbox.min().y()};
    const AlignedHalfPlane<Scalar, 1, true> h3{bbox.max().y()};

    SmallPolygon2<Scalar, 7> result = triangle;
    result = clip_small_poly_by_aligned_half_plane(result, h0);
    result = clip_small_poly_by_aligned_half_plane(result, h1);
    result = clip_small_poly_by_aligned_half_plane(result, h2);
    result = clip_small_poly_by_aligned_half_plane(result, h3);

    return result;
}

#define LA_X_clip_triangle_by_bbox(ValueType, Scalar)        \
    template SmallPolygon2<Scalar, 7> clip_triangle_by_bbox( \
        const SmallPolygon2<Scalar, 3>& triangle,            \
        const Eigen::AlignedBox<Scalar, 2>& bbox);
LA_ATTRIBUTE_SCALAR_X(clip_triangle_by_bbox, 0)

} // namespace lagrange::texproc::internal
