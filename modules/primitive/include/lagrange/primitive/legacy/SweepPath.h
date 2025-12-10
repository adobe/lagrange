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

#include <lagrange/internal/constants.h>
#include <lagrange/legacy/inline.h>
#include <lagrange/utils/assert.h>
#include <lagrange/utils/range.h>
#include <lagrange/utils/safe_cast.h>

#include <Eigen/Geometry>
#include <Eigen/SVD>

#include <algorithm>
#include <numeric>
#include <vector>

namespace lagrange {
namespace primitive {
LAGRANGE_LEGACY_INLINE
namespace legacy {

/**
 * Abstract base class for sweep path.
 *
 * Usage:
 *
 * ```
 *     SweepPath* path = ...;
 *
 *     // Optinal pivot point setting.
 *     path->set_pivot(p);
 *
 *     // Optional twist setting.
 *     path->set_twist_begin(0);
 *     path->set_twist_end(2 * lagrange::internal::pi);
 *
 *     // Optional taper setting.
 *     path->set_taper_begin(1);
 *     path->set_taper_end(0.5);
 *
 *     // Optional depth setting.
 *     path->set_depth_begin(0);
 *     path->set_depth_end(length);
 *
 *     // Optional offset setting.
 *     path->set_offset_function(...);
 *
 *     path->set_num_samples(N);    // Uniform samples along the path.  N>=2.
 *     path->initialize(); // Required.
 *
 *     const auto& transforms = path->get_tranforms();
 *     const auto& offsets = path.get_offsets();
 * ```
 *
 * where `transforms` are used to transform profile curves to cross section
 * curves in a swept surface.
 */
template <typename _Scalar>
class SweepPath
{
public:
    using Scalar = _Scalar;
    using TransformType = Eigen::Transform<Scalar, 3, Eigen::AffineCompact>;
    using PointType = Eigen::Matrix<Scalar, 1, 3>;

    virtual ~SweepPath() = default;

public:
    /**
     * Create a deep copy of itself.
     */
    virtual std::unique_ptr<SweepPath<Scalar>> clone() const = 0;

    /**
     * Generate transformation matrices based on the setting provided.
     * The generated transforms can be retrieved using `get_transforms()` method.
     */
    virtual void initialize() = 0;

    /**
     * Whether the sweep path is closed.
     */
    virtual bool is_closed() const = 0;

    /**
     * The number of samples used to sample along the sweeping path.
     * Each sample corresponds to a new cross section profile curve on the swept
     * surface.
     */
    size_t get_num_samples() const { return m_samples.size(); }

    /**
     * Set the number of samples for uniform sampling of the sweeping path.
     */
    void set_num_samples(size_t n)
    {
        la_runtime_assert(n >= 2, "At least 2 samples is necessary for sweep path!");
        m_samples.resize(n);
        for (size_t i = 0; i < n; i++) {
            m_samples[i] = (Scalar)i / (Scalar)(n - 1);
        }
    }

    /**
     * Samples are always in ascending order going from 0 to 1.
     */
    const std::vector<Scalar>& get_samples() const { return m_samples; }

    /**
     * Set the sample points.
     *
     * @param[in]  samples  Where to sample along the sweep path.  Must be
     *                      in the range [0, 1], and must be sorted in ascending order.
     */
    void set_samples(std::vector<Scalar> samples)
    {
        la_runtime_assert(samples.size() >= 2, "At least 2 samples is necessary for sweep path!");
        m_samples = std::move(samples);
    }

    /**
     * Add samples to the existing samples.  I.e. union `samples` and
     * `m_samples` together while keeping the ascending sorted order.
     *
     * @param[in]  samples  Additional sampling points along the sweep path.  Must be
     *                      sorted in ascending order.
     */
    void add_samples(const std::vector<Scalar>& samples)
    {
        std::vector<Scalar> results;
        results.reserve(m_samples.size() + samples.size());
        std::set_union(
            m_samples.begin(),
            m_samples.end(),
            samples.begin(),
            samples.end(),
            std::back_inserter(results));
        m_samples = std::move(results);
    }

    /**
     * Retrieve the transforms generated using `initialize()`.
     */
    const std::vector<TransformType>& get_transforms() const { return m_transforms; }
    std::vector<TransformType>& get_transforms() { return m_transforms; }

    /**
     * Retrieve the sampled normal offsets.
     */
    std::vector<Scalar> get_offsets() const
    {
        if (has_offsets()) {
            std::vector<Scalar> offsets;
            offsets.reserve(get_num_samples());
            for (auto t : m_samples) {
                offsets.push_back(m_offset_fn(t));
            }
            return offsets;
        } else {
            return {};
        }
    }

    /**
     * Start sweeping at certain depth along the path.  Measured in distance
     * unit.  Default is 0.
     */
    Scalar get_depth_begin() const { return m_depth_begin; }
    void set_depth_begin(Scalar depth) { m_depth_begin = depth; }

    /**
     * Stop sweeping at certain depth along the path.  Measured in distance
     * unit.  Default is 1.
     */
    Scalar get_depth_end() const { return m_depth_end; }
    void set_depth_end(Scalar depth) { m_depth_end = depth; }

    /**
     * Twisting angle at the beginning of the sweep path.
     * Unit: radian, default: 0.
     */
    Scalar get_twist_begin() const { return m_twist_begin; }
    void set_twist_begin(Scalar twist) { m_twist_begin = twist; }

    /**
     * Twisting angle at the end of the sweep path.
     * Unit: radian, default: 0.
     */
    Scalar get_twist_end() const { return m_twist_end; }
    void set_twist_end(Scalar twist) { m_twist_end = twist; }

    /**
     * Scaling factor at the beginning of the sweep path.  Default: 1.
     */
    Scalar get_taper_begin() const { return m_taper_begin; }
    void set_taper_begin(Scalar taper) { m_taper_begin = taper; }

    /**
     * Scaling factor at the beginning of the sweep path.  Default: 1.
     */
    Scalar get_taper_end() const { return m_taper_end; }
    void set_taper_end(Scalar taper) { m_taper_end = taper; }

    /**
     * Twisting and tapering are all with respect to a pivot point.
     * This method sets the pivot point. The default pivot point is the origin.
     */
    const PointType& get_pivot() const { return m_pivot; }
    void set_pivot(const PointType& p) { m_pivot = p; }

    /**
     * Offset function provides a mapping from the relative depth (from 0 to 1)
     * to a normal offset amount (measured in 3D Euclidean distance).
     */
    void set_offset_fn(std::function<Scalar(Scalar)> fn) { m_offset_fn = std::move(fn); }
    bool has_offsets() const { return bool(m_offset_fn); }

    /**
     * Sometimes one may want to sweep a normalized profile curve, and update
     * normalization from time to time. This method sets the normalization
     * transform.  By default, normalization transform is identity.
     *
     * @warning: Pivot point is a post-normalization quantity, and it will not
     * be updated when normalization changes.
     */
    void set_normalization_transform(const TransformType& transform)
    {
        m_normalization = transform;
    }
    const TransformType& get_normalization_transform() const { return m_normalization; }

    /**
     * Check if two sweep paths are the same.
     */
    virtual bool operator==(const SweepPath<Scalar>& other) const
    {
        const size_t N = m_transforms.size();
        if (N != other.m_transforms.size()) return false;

        constexpr Scalar TOL = std::numeric_limits<Scalar>::epsilon() * 100;
        if ((m_pivot - other.m_pivot).norm() > TOL) return false;
        if (std::abs(m_depth_begin - other.m_depth_begin) > TOL) return false;
        if (std::abs(m_depth_end - other.m_depth_end) > TOL) return false;
        if (std::abs(m_twist_begin - other.m_twist_begin) > TOL) return false;
        if (std::abs(m_twist_end - other.m_twist_end) > TOL) return false;
        if (std::abs(m_taper_begin - other.m_taper_begin) > TOL) return false;
        if (std::abs(m_taper_end - other.m_taper_end) > TOL) return false;

        if (m_samples != other.m_samples) {
            return false;
        }

        if (get_offsets() != other.get_offsets()) {
            return false;
        }

        // Note: change in normalization transform is ok.
        return true;
    }

protected:
    void clone_settings(SweepPath<Scalar>& other) const
    {
        other.set_depth_begin(get_depth_begin());
        other.set_depth_end(get_depth_end());
        other.set_twist_begin(get_twist_begin());
        other.set_twist_end(get_twist_end());
        other.set_taper_begin(get_taper_begin());
        other.set_taper_end(get_taper_end());
        other.set_pivot(get_pivot());
        other.set_samples(get_samples());
        other.set_offset_fn(m_offset_fn);
    }

protected:
    std::vector<TransformType> m_transforms;
    std::vector<Scalar> m_samples; ///< should be sorted in ascending order from 0 to 1.
    TransformType m_normalization = TransformType::Identity();
    Scalar m_depth_begin = 0;
    Scalar m_depth_end = 1;
    Scalar m_twist_begin = 0;
    Scalar m_twist_end = 0;
    Scalar m_taper_begin = 1;
    Scalar m_taper_end = 1;
    PointType m_pivot{0, 0, 0};
    std::function<Scalar(Scalar)> m_offset_fn;
};


template <typename _Scalar>
class LinearSweepPath final : public SweepPath<_Scalar>
{
public:
    using Parent = SweepPath<_Scalar>;
    using Scalar = typename Parent::Scalar;

public:
    /**
     * Constructor.
     *
     * @param[in]  dir  Linear path direction.
     */
    LinearSweepPath(Eigen::Matrix<Scalar, 1, 3> dir)
    {
        m_direction = dir.normalized();
        la_runtime_assert(
            m_direction.array().isFinite().all(),
            "Invalid linear extrusion path direction");
        Parent::set_num_samples(2);
    }
    ~LinearSweepPath() = default;

public:
    void initialize() override
    {
        const size_t N = Parent::get_num_samples();
        la_runtime_assert(N >= 2, "Extrusion path must consist of at least 2 samples");
        Parent::m_transforms.clear();
        Parent::m_transforms.resize(N);

        for (size_t i = 0; i < N; i++) {
            const Scalar t = Parent::m_samples[i];
            Eigen::AngleAxis<Scalar> R(
                Parent::m_twist_begin * (1 - t) + Parent::m_twist_end * t,
                m_direction);
            Eigen::Transform<Scalar, 3, Eigen::AffineCompact> S;
            S.setIdentity();
            S(0, 0) = (1 - t) * Parent::m_taper_begin + t * Parent::m_taper_end;
            S(1, 1) = (1 - t) * Parent::m_taper_begin + t * Parent::m_taper_end;

            Parent::m_transforms[i].setIdentity();
            Parent::m_transforms[i].translate(Parent::m_pivot.transpose());
            Parent::m_transforms[i].translate(
                m_direction.transpose() *
                (Parent::m_depth_begin * (1 - t) + Parent::m_depth_end * t));
            Parent::m_transforms[i].rotate(R);
            Parent::m_transforms[i] = Parent::m_transforms[i] * S;
            Parent::m_transforms[i].translate(-Parent::m_pivot.transpose());

            // Apply normalization transform.
            Parent::m_transforms[i] = Parent::m_normalization.inverse() * Parent::m_transforms[i] *
                                      Parent::m_normalization;
        }
    }

    std::unique_ptr<Parent> clone() const override
    {
        auto r = std::make_unique<LinearSweepPath<Scalar>>(m_direction);
        Parent::clone_settings(*r);
        r->initialize();
        return r;
    }

    bool is_closed() const override
    {
        // Linear path will never be closed.
        return false;
    }

    bool operator==(const SweepPath<Scalar>& other) const override
    {
        if (const auto* other_linear = dynamic_cast<const LinearSweepPath<Scalar>*>(&other)) {
            if (Parent::operator==(other)) {
                constexpr Scalar TOL = std::numeric_limits<Scalar>::epsilon() * 100;
                return (m_direction - other_linear->m_direction).norm() < TOL;
            }
        }
        return false;
    }

private:
    Eigen::Matrix<Scalar, 1, 3> m_direction;
};


/**
 * In addition to the member function provided in SweepPath,
 * CircularArcSweepPath also provide the following handy methods:
 *
 * ```
 *     path->set_angle_begin(0);   // Set the arc angle at the beginning.
 *     path->set_angle_end(lagrange::internal::pi);  // Set the arc angle at the end.
 * ```
 */
template <typename _Scalar>
class CircularArcSweepPath final : public SweepPath<_Scalar>
{
public:
    using Parent = SweepPath<_Scalar>;
    using Scalar = typename Parent::Scalar;

public:
    /**
     * Circular arc path constructor.  The circular arc is embedded in a plane
     * perpendicular to the XY plane.
     *
     * @param[in] radius  Radius of the circle.
     * @param[in] theta   Rotation angle around Z axis.  0 means the circular
     *                    arc is embedded in the XZ plane.
     */
    CircularArcSweepPath(Scalar radius, Scalar theta)
        : m_radius(radius)
        , m_theta(theta)
    {
        // Initialize setting based on common use case.
        Parent::set_num_samples(32);
        Parent::set_depth_begin(0);
        Parent::set_depth_end(2 * lagrange::internal::pi * radius);
        la_runtime_assert(
            m_radius >= 0,
            "Negaative radius is not supported in CircularArcSweepPath.");
    }
    ~CircularArcSweepPath() = default;

public:
    /**
     * Set starting sweeping angle.  This is an alternative way of setting
     * depth at the beginning of the sweep.
     */
    void set_angle_begin(Scalar theta) { Parent::set_depth_begin(theta * m_radius); }

    /**
     * Set ending sweeping angle.  This is an alternative way of setting
     * depth at the end of the sweep.
     */
    void set_angle_end(Scalar theta) { Parent::set_depth_end(theta * m_radius); }

    void initialize() override
    {
        const size_t N = Parent::get_num_samples();
        la_runtime_assert(N >= 2, "Extrusion path must consist of at least 2 samples");
        Parent::m_transforms.clear();
        Parent::m_transforms.resize(N);

        Eigen::Matrix<Scalar, 1, 3> radial_dir(std::cos(m_theta), std::sin(m_theta), 0);
        Eigen::Matrix<Scalar, 1, 3> z_axis(0, 0, 1);
        Eigen::Matrix<Scalar, 1, 3> normal = z_axis.cross(radial_dir).normalized();

        const Scalar angle_begin = (m_radius == 0) ? 0 : Parent::m_depth_begin / m_radius;
        const Scalar angle_end = (m_radius == 0) ? 0 : Parent::m_depth_end / m_radius;

        for (size_t i = 0; i < N; i++) {
            const Scalar t = Parent::m_samples[i];
            Eigen::AngleAxis<Scalar> R(
                Parent::m_twist_begin * (1 - t) + Parent::m_twist_end * t,
                z_axis);
            Eigen::Transform<Scalar, 3, Eigen::AffineCompact> S;
            S.setIdentity();
            S(0, 0) = (1 - t) * Parent::m_taper_begin + t * Parent::m_taper_end;
            S(1, 1) = (1 - t) * Parent::m_taper_begin + t * Parent::m_taper_end;

            Parent::m_transforms[i].setIdentity();
            Parent::m_transforms[i].translate(Parent::m_pivot.transpose());

            Parent::m_transforms[i].translate(radial_dir.transpose() * m_radius);
            Eigen::AngleAxis<Scalar> R2(angle_begin * (1 - t) + angle_end * t, normal);
            Parent::m_transforms[i].rotate(R2);
            Parent::m_transforms[i].translate(-radial_dir.transpose() * m_radius);

            Parent::m_transforms[i].rotate(R);
            Parent::m_transforms[i] = Parent::m_transforms[i] * S;
            Parent::m_transforms[i].translate(-Parent::m_pivot.transpose());

            // Apply normalization transform.
            Parent::m_transforms[i] = Parent::m_normalization.inverse() * Parent::m_transforms[i] *
                                      Parent::m_normalization;
        }

        if (is_closed()) {
            Parent::m_transforms.back() = Parent::m_transforms.front();
        }
    }

    std::unique_ptr<Parent> clone() const override
    {
        auto r = std::make_unique<CircularArcSweepPath<Scalar>>(m_radius, m_theta);
        Parent::clone_settings(*r);
        r->initialize();
        return r;
    }

    bool is_closed() const override
    {
        constexpr Scalar two_pi = static_cast<Scalar>(lagrange::internal::pi * 2);
        constexpr Scalar point_five_degree = static_cast<Scalar>(lagrange::internal::pi / 360);

        const auto angle_begin = Parent::get_depth_begin() / m_radius;
        const auto angle_end = Parent::get_depth_end() / m_radius;
        const Scalar angle_diff = angle_end - angle_begin;
        const int winding = static_cast<int>(std::round(angle_diff / two_pi));

        // A circular path is considered closed if the sweeping angle is
        // a multiple of 2 * lagrange::internal::pi with error tolerance ±0.1 degrees.
        return (winding != 0) && std::abs(angle_diff - winding * two_pi) < point_five_degree;
    }

    bool operator==(const SweepPath<Scalar>& other) const override
    {
        if (const auto* other_circular =
                dynamic_cast<const CircularArcSweepPath<Scalar>*>(&other)) {
            if (Parent::operator==(other)) {
                constexpr Scalar TOL = std::numeric_limits<Scalar>::epsilon() * 100;
                return std::abs(m_radius - other_circular->m_radius) < TOL &&
                       std::abs(m_theta - other_circular->m_theta) < TOL;
            }
        }
        return false;
    }

private:
    Scalar m_radius;
    Scalar m_theta;
};


/**
 * Create sweep path based on a polyline.
 */
template <typename _VertexArray>
class PolylineSweepPath final : public SweepPath<typename _VertexArray::Scalar>
{
public:
    using Scalar = typename _VertexArray::Scalar;
    using VertexArray = _VertexArray;
    using Parent = SweepPath<Scalar>;
    using Index = Eigen::Index;

public:
    /**
     * Polyline sweep path constructor.
     *
     * @param[in]  polyline  The polyline path.
     *
     * @note
     * A polylline is considered "closed" if the first vertex and the last
     * vertex are the same.
     *
     * @note
     * By default, the generated PolylineSweepPath object will cover the entire
     * polyline and transformations are sampled at the vertices on the polyline.
     * The sweep path can be a portion of the polyline by `set_depth_begin()`
     * and `set_depth_end()`.  The sampling rate can also be changed using
     * `set_num_samples()`.
     */
    PolylineSweepPath(const VertexArray& polyline)
        : m_polyline(polyline)
    {
        la_runtime_assert(polyline.cols() == 3, "Sweep path must be 3D path.");
        la_runtime_assert(polyline.rows() > 1, "Sweep path must consist of at least 2 points!");

        Index num_lines = polyline.rows() - 1;
        m_lengths.resize(num_lines);
        for (auto i : range<Index>(1, num_lines + 1)) {
            m_lengths[i - 1] = (polyline.row(i) - polyline.row(i - 1)).norm();
        }
        const Scalar total_length = std::accumulate(m_lengths.begin(), m_lengths.end(), Scalar(0));

        // Initialize some settings based on common use case.
        Parent::set_num_samples(num_lines + 1);
        Parent::set_depth_begin(0);
        Parent::set_depth_end(total_length);
    }
    ~PolylineSweepPath() = default;

public:
    void initialize() override
    {
        using TransformType = Eigen::Transform<Scalar, 3, Eigen::AffineCompact>;
        using PointType = Eigen::Matrix<Scalar, 1, 3>;

        const Index N = Parent::get_num_samples();
        const Index M = static_cast<Index>(m_polyline.rows());
        const bool path_closed = is_polyline_closed();
        const Index m = path_closed ? (M - 1) : M;
        const Eigen::Matrix<Scalar, 1, 3> z_axis(0, 0, 1);

        /**
         * Apply normalization, twist and taper transformations to a given
         * transform.
         */
        auto update_transform = [&](TransformType& transform, Index i) {
            const Scalar t = (Scalar)i / (Scalar)(M - 1);
            Eigen::AngleAxis<Scalar> R(
                Parent::m_twist_begin * (1 - t) + Parent::m_twist_end * t,
                z_axis);
            Eigen::Transform<Scalar, 3, Eigen::AffineCompact> S;
            S.setIdentity();
            S(0, 0) = (1 - t) * Parent::m_taper_begin + t * Parent::m_taper_end;
            S(1, 1) = (1 - t) * Parent::m_taper_begin + t * Parent::m_taper_end;

            transform.pretranslate(Parent::m_pivot.transpose());
            transform.rotate(R); // twist
            transform = transform * S; // taper
            transform.translate(-Parent::m_pivot.transpose());
            transform = Parent::m_normalization.inverse() * transform * Parent::m_normalization;
        };

        auto generate_node_transforms = [&]() {
            std::vector<TransformType> transforms;
            transforms.reserve(M);

            // Initialize first transform.
            transforms.push_back(TransformType::Identity());
            update_transform(transforms.front(), 0);

            Eigen::Quaternion<Scalar> rotation, I;
            rotation.setIdentity();
            I.setIdentity();
            PointType translation;
            translation.setZero();

            if (path_closed) {
                PointType v1 = m_polyline.row(0) - m_polyline.row(m - 1);
                PointType v2 = m_polyline.row(1) - m_polyline.row(0);
                rotation = Eigen::Quaternion<Scalar>::FromTwoVectors(v1, v2).slerp(0.5f, I);
            }

            // Compute vertex positions.
            for (Index i = 1; i < m; i++) {
                PointType v1 = m_polyline.row(i) - m_polyline.row(i - 1);
                PointType v2 = v1;
                if (path_closed || i < m - 1) {
                    v2 = m_polyline.row((i + 1) % m) - m_polyline.row(i);
                }
                auto half_r = Eigen::Quaternion<Scalar>::FromTwoVectors(v1, v2).slerp(0.5f, I);
                translation += v1;
                rotation = half_r * rotation;

                TransformType transform;
                transform.setIdentity();
                transform.translate(translation.transpose());
                transform.rotate(rotation);
                update_transform(transform, i);
                transforms.push_back(std::move(transform));

                rotation = half_r * rotation;
            }

            if (path_closed) {
                transforms.push_back(TransformType::Identity());
                update_transform(transforms.back(), M - 1);
            }

            using Matrix = Eigen::Matrix<Scalar, 3, 3>;
            using Vector = Eigen::Matrix<Scalar, 3, 1>;
            std::vector<std::tuple<Matrix, Matrix, Vector>> decompositions;
            decompositions.reserve(M);
            for (const auto& transform : transforms) {
                Matrix R, S;
                Vector T;
                transform.computeRotationScaling(&R, &S);
                T = transform.translation();
                decompositions.emplace_back(R, S, T);
            }

            return decompositions;
        };

        auto node_transforms = generate_node_transforms();
        Parent::m_transforms.clear();
        Parent::m_transforms.reserve(N);
        Index curr_span = 0;
        Scalar curr_depth = 0;
        Scalar next_depth = m_lengths[0];

        Eigen::Matrix<Scalar, 3, 3> R0, R1, R;
        Eigen::Matrix<Scalar, 3, 3> S0, S1, S;
        Eigen::Matrix<Scalar, 3, 1> T0, T1, T;

        for (Index i = 0; i < N; i++) {
            const Scalar t = Parent::m_samples[i];
            const Scalar d = Parent::get_depth_begin() * (1 - t) + Parent::get_depth_end() * t;

            while (next_depth < d && curr_span < M - 1) {
                curr_span++;
                curr_depth = next_depth;
                next_depth += m_lengths[curr_span];
            }

            Scalar tt = (d - curr_depth) / (next_depth - curr_depth);
            tt = std::max<Scalar>(0, std::min<Scalar>(1, tt));

            std::tie(R0, S0, T0) = node_transforms[curr_span];
            std::tie(R1, S1, T1) = node_transforms[curr_span + 1];

            R = Eigen::Quaternion<Scalar>(R0)
                    .slerp(tt, Eigen::Quaternion<Scalar>(R1))
                    .toRotationMatrix();
            S = S0 * (1 - tt) + S1 * tt;
            T = T0 * (1 - tt) + T1 * tt;

            TransformType transform;
            transform.setIdentity();
            transform.linear() = R * S;
            transform.translation() = T;
            Parent::m_transforms.push_back(std::move(transform));
        }

        if (is_closed()) {
            Parent::m_transforms.back() = Parent::m_transforms.front();
        }
    }

    std::unique_ptr<Parent> clone() const override
    {
        auto r = std::make_unique<PolylineSweepPath<VertexArray>>(m_polyline);
        Parent::clone_settings(*r);
        r->initialize();
        return r;
    }

    bool is_closed() const override
    {
        if (is_polyline_closed()) {
            constexpr Scalar TOL = std::numeric_limits<Scalar>::epsilon() * 100;
            const Scalar total_length =
                std::accumulate(m_lengths.begin(), m_lengths.end(), Scalar(0));
            return std::abs(Parent::m_depth_begin) < TOL &&
                   std::abs(Parent::m_depth_end - total_length) < TOL;
        }
        return false;
    }

    bool operator==(const SweepPath<Scalar>& other) const override
    {
        if (const auto* other_polyline =
                dynamic_cast<const PolylineSweepPath<VertexArray>*>(&other)) {
            if (Parent::operator==(other)) {
                constexpr Scalar TOL = std::numeric_limits<Scalar>::epsilon() * 100;
                return (
                    (m_polyline - other_polyline->m_polyline).template lpNorm<Eigen::Infinity>() <
                    TOL);
            }
        }
        return false;
    }

protected:
    /**
     * @note: A closed polyline does not necessarily mean a closed sweep path
     * because the sweep path may not cover the entire polyline depending on the
     * depth settings.
     */
    bool is_polyline_closed() const
    {
        constexpr Scalar TOL = std::numeric_limits<Scalar>::epsilon() * 100;
        const Index M = static_cast<Index>(m_polyline.rows());
        return (m_polyline.row(0) - m_polyline.row(M - 1)).norm() < TOL;
    }

private:
    VertexArray m_polyline;
    std::vector<Scalar> m_lengths;
};


} // namespace legacy
} // namespace primitive
} // namespace lagrange
