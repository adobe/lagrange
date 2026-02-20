/*
 * Copyright 2025 Adobe. All rights reserved.
 * This file is licensed to you under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License. You may obtain a copy
 * of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under
 * the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
 * OF ANY KIND, either express or implied. See the License for the specific language
 * governing permissions and limitations under the License.
 */

#include <lagrange/Logger.h>
#include <lagrange/SurfaceMeshTypes.h>
#include <lagrange/primitive/SweepOptions.h>
#include <lagrange/primitive/api.h>

#include <lagrange/utils/assert.h>

#include <lagrange/internal/constants.h>
#include <cmath>

namespace lagrange::primitive {

template <typename Scalar>
std::vector<typename SweepOptions<Scalar>::Transform> SweepOptions<Scalar>::sample_transforms()
    const
{
    size_t num_samples = m_num_samples;
    la_runtime_assert(num_samples >= 2, "Number of samples must be at least 2.");
    la_runtime_assert(
        m_domain[1] > m_domain[0],
        "Invalid domain: the end value must be greater than the start value.");
    la_runtime_assert(m_position_fn, "Position function must be set before sampling transforms.");
    la_runtime_assert(m_frame_fn, "Frame function must be set before sampling transforms.");

    std::vector<Transform> transforms(num_samples);
    Eigen::Matrix<Scalar, 1, 3> z_axis(0, 0, 1);
    Transform normalization_inv = m_normalization.inverse();

    for (size_t i = 0; i < num_samples; i++) {
        const Scalar t = m_domain[0] + (m_domain[1] - m_domain[0]) * (Scalar)i / (num_samples - 1);

        const Point p = m_position_fn(t);
        const Frame frame = m_frame_fn(t);
        const Scalar twist = m_twist_fn ? m_twist_fn(t) : 0;
        const Scalar taper = m_taper_fn ? m_taper_fn(t) : 1;

        transforms[i].setIdentity();

        transforms[i].translate(m_pivot.transpose());

        transforms[i].translate(p.transpose());
        transforms[i].rotate(frame);

        transforms[i].rotate(Eigen::AngleAxis<Scalar>(twist, z_axis));
        transforms[i].scale(taper);

        transforms[i].translate(-m_pivot.transpose());

        transforms[i] = normalization_inv * transforms[i] * m_normalization;
    }

    return transforms;
}

template <typename Scalar>
typename SweepOptions<Scalar>::Transform SweepOptions<Scalar>::sample_transform(Scalar t) const
{
    la_runtime_assert(m_position_fn, "Position function must be set before sampling transforms.");
    la_runtime_assert(m_frame_fn, "Frame function must be set before sampling transforms.");

    Eigen::Matrix<Scalar, 1, 3> z_axis(0, 0, 1);
    Transform normalization_inv = m_normalization.inverse();

    Transform transform;
    const Point p = m_position_fn(t);
    const Frame frame = m_frame_fn(t);
    const Scalar twist = m_twist_fn ? m_twist_fn(t) : 0;
    const Scalar taper = m_taper_fn ? m_taper_fn(t) : 1;

    transform.setIdentity();

    transform.translate(m_pivot.transpose());

    transform.translate(p.transpose());
    transform.rotate(frame);

    transform.rotate(Eigen::AngleAxis<Scalar>(twist, z_axis));
    transform.scale(taper);

    transform.translate(-m_pivot.transpose());

    transform = normalization_inv * transform * m_normalization;

    return transform;
}

template <typename Scalar>
std::vector<Scalar> SweepOptions<Scalar>::sample_offsets() const
{
    size_t num_samples = m_num_samples;
    la_runtime_assert(num_samples >= 2, "Number of samples must be at least 2.");
    la_runtime_assert(
        m_domain[1] > m_domain[0],
        "Invalid domain: the end value must be greater than the start value.");

    std::vector<Scalar> offsets(num_samples, 0);

    if (m_offset_fn) {
        for (size_t i = 0; i < num_samples; i++) {
            const Scalar t =
                m_domain[0] + (m_domain[1] - m_domain[0]) * (Scalar)i / (num_samples - 1);
            offsets[i] = m_offset_fn(t);
        }
    }
    return offsets;
}

template <typename Scalar>
Scalar SweepOptions<Scalar>::sample_offset(Scalar t) const
{
    la_runtime_assert(m_offset_fn, "Offset function must be set before sampling offsets.");
    return m_offset_fn(t);
}

template <typename Scalar>
SweepOptions<Scalar>
SweepOptions<Scalar>::linear_sweep(const Point& from, const Point& to, bool follow_tangent)
{
    SweepOptions<Scalar> setting;
    setting.set_num_samples(2);

    setting.set_position_function([from, to](Scalar t) { return from * (1 - t) + to * t; });

    if (follow_tangent) {
        Point z_axis(0, 0, 1);
        Point direction = (to - from).normalized();
        auto R = Eigen::Quaternion<Scalar>::FromTwoVectors(z_axis, direction).matrix();
        setting.set_frame_function([R](Scalar) { return R; });
    } else {
        setting.set_frame_function([](Scalar) { return Eigen::Matrix<Scalar, 3, 3>::Identity(); });
    }

    return setting;
}

template <typename Scalar>
SweepOptions<Scalar> SweepOptions<Scalar>::circular_sweep(
    const Point& p,
    const Point& axis,
    Scalar angle,
    bool follow_tangent)
{
    SweepOptions<Scalar> setting;
    setting.set_num_samples(32);

    logger().debug("center: [{}, {}, {}]", p[0], p[1], p[2]);
    logger().debug("  axis: [{}, {}, {}]", axis[0], axis[1], axis[2]);

    setting.set_position_function([p, axis](Scalar t) {
        Scalar theta = 2 * lagrange::internal::pi * t;
        auto R = Eigen::AngleAxis<Scalar>(theta, axis.stableNormalized());
        Point q = (R * p.transpose()).transpose();
        return q;
    });

    if (follow_tangent) {
        setting.set_frame_function([axis](Scalar t) {
            Scalar theta = 2 * lagrange::internal::pi * t;
            auto R = Eigen::AngleAxis<Scalar>(theta, axis.stableNormalized()).matrix();
            return R;
        });
    } else {
        setting.set_frame_function([](Scalar) { return Eigen::Matrix<Scalar, 3, 3>::Identity(); });
    }

    {
        // Check if the sweep is periodic. i.e. the sweep angle should be a non-zero multiple of 2 *
        // pi for periodic sweeps.
        constexpr double two_pi = 2 * lagrange::internal::pi;
        constexpr double tol = 1e-6;
        double r = std::fmod(static_cast<double>(angle), two_pi);
        // Normalize remainder to [-π, π]
        if (r > lagrange::internal::pi) r -= two_pi;
        if (r < -lagrange::internal::pi) r += two_pi;
        setting.set_periodic(std::abs(static_cast<double>(angle)) > tol && std::abs(r) < tol);
    }

    return setting;
}

#define LA_X_sweep_setting(_, Scalar) template class LA_PRIMITIVE_API SweepOptions<Scalar>;

LA_SURFACE_MESH_SCALAR_X(sweep_setting, 0)

} // namespace lagrange::primitive
