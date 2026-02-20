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
#pragma once

#include <lagrange/internal/constants.h>
#include <lagrange/utils/assert.h>

#include <Eigen/Geometry>

#include <array>
#include <functional>
#include <vector>

namespace lagrange::primitive {

/// @addtogroup module-primitive
/// @{

/**
 * Configuration class for sweep operations on 3D geometry.
 *
 * SweepOptions provides a flexible framework for defining sweep transformations
 * that can be applied to 2D profiles to generate 3D geometry. It supports various
 * types of sweeps including linear and circular sweeps, with additional control
 * over frame orientation, twist, taper, and offset along the sweep path.
 *
 * @tparam Scalar The scalar type used for computations (e.g., float, double).
 */
template <typename Scalar>
class SweepOptions
{
public:
    using Point = Eigen::Matrix<Scalar, 1, 3>;
    using Frame = Eigen::Matrix<Scalar, 3, 3>;
    using Transform = Eigen::Transform<Scalar, 3, Eigen::AffineCompact>;

public:
    /**
     * Creates a linear sweep configuration from one point to another.
     *
     * @param from The starting point of the linear sweep relative to the pivot.
     * @param to The ending point of the linear sweep relative to the pivot.
     * @param follow_tangent Whether the sweep frame should follow the tangent direction.
     * @return A SweepOptions configured for linear sweep.
     */
    static SweepOptions<Scalar>
    linear_sweep(const Point& from, const Point& to, bool follow_tangent = true);

    /**
     * Creates a circular sweep configuration around a specified axis.
     *
     * @param p A point defining a point on the circular sweep path relative to the pivot.
     * @param axis The axis of rotation for the circular sweep (should be normalized).
     * @param angle The angle of the circular sweep in radians (default is 2 * pi for a full
     * circle).
     * @param follow_tangent Whether the sweep frame should follow the tangent direction.
     * @return A SweepOptions configured for circular sweep.
     *
     * @note The sweep setting will be periodic if the angle is a non-zero multiple of 2 * pi.
     * @warning The axis vector should be normalized for predictable results.
     *
     * Example:
     * @code
     * auto sweep = SweepOptions<float>::circular_sweep(
     *     Point(1, 0, 0),  // radius 1 circle in XY plane
     *     Point(0, 0, 1)   // around Z axis
     * );
     * @endcode
     */
    static SweepOptions<Scalar> circular_sweep(
        const Point& p,
        const Point& axis,
        Scalar angle = static_cast<Scalar>(2 * lagrange::internal::pi),
        bool follow_tangent = true);

public:
    /**
     * Samples transformation matrices along the sweep path.
     *
     * @return A vector of transformation matrices with size equal to get_num_samples().
     */
    std::vector<Transform> sample_transforms() const;

    /**
     * Samples the transformation matrix at a specific parameter t.
     *
     * @param t The parameter value in the domain.
     *
     * @return The transformation matrix at the specified parameter t.
     */
    Transform sample_transform(Scalar t) const;

    /**
     * Samples offset values along the sweep path.
     *
     * @return A vector of offset values sampled along the sweep path.
     */
    std::vector<Scalar> sample_offsets() const;

    /**
     * Samples the offset value at a specific parameter t.
     *
     * @param t The parameter value in the domain.
     *
     * @return The offset value at the specified parameter t.
     */
    Scalar sample_offset(Scalar t) const;

    /**
     * Sets the pivot point for the sweep transformation.
     *
     * Pivot point is the relative origin for the sweep transformations. It is typically set at the
     * center of the profile being swept. By default, it is set to the origin (0, 0, 0).
     *
     * @param pivot The pivot point to use for transformations.
     *
     * @see @ref get_pivot, @ref set_normalization
     */
    void set_pivot(const Point& pivot) { m_pivot = pivot; }

    /**
     * Gets the current pivot point.
     *
     * @return The current pivot point.
     */
    const Point& get_pivot() const { return m_pivot; }

    /**
     * Sets the normalization transformation applied to the sweep.
     *
     * Normalization is typically used to normalize the profile curve to fit in a unit box centered
     * at the origin. All sweep transformations will be relative to this normalized space. The
     * default normalization is the identity transformation, meaning no normalization is applied.
     *
     * @param normalization The normalization transformation to apply.
     * @note The transformation should be invertible for proper sweep generation.
     * @warning Transformations with zero determinant will cause undefined behavior.
     */
    void set_normalization(const Transform& normalization) { m_normalization = normalization; }

    /**
     * Gets the current normalization transformation.
     *
     * @return The current normalization transformation.
     */
    const Transform& get_normalization() const { return m_normalization; }

    /**
     * Sets the number of samples to use along the sweep path.
     *
     * The default number of samples is 16.
     *
     * @param num_samples The number of samples (must be at least 2).
     * @throws std::runtime_error if num_samples is less than 2.
     */
    void set_num_samples(size_t num_samples)
    {
        la_runtime_assert(num_samples >= 2, "At least 2 samples are required.");
        m_num_samples = num_samples;
    }

    /**
     * Gets the current number of samples.
     *
     * @return The current number of samples.
     */
    size_t get_num_samples() const { return m_num_samples; }

    /**
     * Sets whether the sweep should be treated as periodic.
     *
     * A sweep is periodic (with period 1) if all transformation functions satisfy:
     * f(t) = f(t + 1) for any t, where f represents position, frame, twist, taper, or offset
     * functions. This is useful for closed sweep paths like circles or loops.
     *
     * @param periodic True if the sweep is periodic (e.g., closed loops).
     * @note When periodic=true, sampling will ensure proper connectivity at domain boundaries.
     */
    void set_periodic(bool periodic) { m_periodic = periodic; }

    /**
     * Checks if the sweep is configured as periodic.
     *
     * @return True if the sweep is periodic, false otherwise.
     */
    bool is_periodic() const { return m_periodic; }

    /**
     * Sets the parameter domain for sampling transformations.
     *
     * @param domain A 2-element array defining the start and end of the parameter domain.
     *
     * @note The domain must satisfy domain[0] < domain[1].
     */
    void set_domain(std::array<Scalar, 2> domain)
    {
        la_runtime_assert(
            domain[0] < domain[1],
            "Invalid domain: the end value must be greater than the start value.");
        m_domain = std::move(domain);
    }

    /**
     * Gets the current parameter domain for sampling transformations.
     *
     * @return A 2-element array representing the start and end of the parameter domain.
     */
    const std::array<Scalar, 2>& get_domain() const { return m_domain; }

    /**
     * Checks if the sweep is closed.
     *
     * A sweep is considered closed if it is periodic and the total length of the sweep path
     * is equal to 1 up to floating point error.
     *
     * @return True if the sweep is closed, false otherwise.
     */
    bool is_closed() const
    {
        return is_periodic() && std::abs(m_domain[1] - m_domain[0] - 1) < static_cast<Scalar>(1e-6);
    }

    /**
     * Sets the position function that defines the sweep path.
     *
     * @param fn A function that maps parameter values to 3D positions along the sweep path.
     */
    void set_position_function(std::function<Point(Scalar)> fn) { m_position_fn = std::move(fn); }

    /**
     * Checks if a position function has been set.
     *
     * @return True if a position function is defined, false otherwise.
     */
    bool has_positions() const { return bool(m_position_fn); }

    /**
     * Sets the frame function that defines orientation along the sweep path.
     *
     * The frame function defines a local coordinate system at each point along the sweep path.
     * Each frame should be orthonormal with determinant +1. The last column/basis of the frame
     * matrix should represent the tangent vector of the sweep path at that point.
     *
     * @param fn A function that maps parameter values in [0,1] to 3x3 orthonormal matrices.
     */
    void set_frame_function(std::function<Frame(Scalar)> fn) { m_frame_fn = std::move(fn); }

    /**
     * Checks if a frame function has been set.
     *
     * @return True if a frame function is defined, false otherwise.
     */
    bool has_frames() const { return bool(m_frame_fn); }

    /**
     * Sets the twist function that defines rotation around the sweep path.
     *
     * The twist function applies additional rotation around the sweep path tangent vector.
     * Twist values are interpreted as radians of rotation.
     *
     * @param fn A function that maps parameter values in [0,1] to twist angles in radians.
     */
    void set_twist_function(std::function<Scalar(Scalar)> fn) { m_twist_fn = std::move(fn); }

    /**
     * Checks if a twist function has been set.
     *
     * @return True if a twist function is defined, false otherwise.
     */
    bool has_twists() const { return bool(m_twist_fn); }

    /**
     * Sets the taper function that defines scaling along the sweep path.
     *
     * The taper function applies uniform scaling to the profile at each point along the sweep.
     * A taper value of 1.0 means no scaling, values > 1.0 expand, values < 1.0 contract.
     *
     * @param fn A function that maps parameter values in [0,1] to positive scale factors.
     */
    void set_taper_function(std::function<Scalar(Scalar)> fn) { m_taper_fn = std::move(fn); }

    /**
     * Checks if a taper function has been set.
     *
     * @return True if a taper function is defined, false otherwise.
     */
    bool has_tapers() const { return bool(m_taper_fn); }

    /**
     * Sets the offset function that defines offsets along the sweep path.
     *
     * @param fn A function that maps parameter values to offset distances.
     */
    void set_offset_function(std::function<Scalar(Scalar)> fn) { m_offset_fn = std::move(fn); }

    /**
     * Checks if an offset function has been set.
     *
     * @return True if an offset function is defined, false otherwise.
     */
    bool has_offsets() const { return bool(m_offset_fn); }

protected:
    /// The pivot point for sweep transformations
    Point m_pivot = Point::Zero();
    /// The normalization transformation applied to the sweep
    Transform m_normalization = Transform::Identity();
    /// The number of samples along the sweep path
    size_t m_num_samples = 16;
    /// Whether the sweep is periodic (closed loop)
    bool m_periodic = false;
    /// The parameter domain for sampling transformations
    std::array<Scalar, 2> m_domain = {0, 1};

    /// Function defining positions along the sweep path
    std::function<Point(Scalar)> m_position_fn;
    /// Function defining frame orientations along the sweep path
    std::function<Frame(Scalar)> m_frame_fn;
    /// Function defining twist angles along the sweep path
    std::function<Scalar(Scalar)> m_twist_fn;
    /// Function defining taper scale factors along the sweep path
    std::function<Scalar(Scalar)> m_taper_fn;
    /// Function defining offsets along the sweep path
    std::function<Scalar(Scalar)> m_offset_fn;
};

/// @}

} // namespace lagrange::primitive
