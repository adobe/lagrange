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
#include <lagrange/internal/clipped_triangle_circumcenter.h>

#include <lagrange/testing/common.h>

#include <Eigen/Core>

TEST_CASE(
    "clipped_triangle_circumcenter: acute triangle returns interior circumcenter",
    "[core][clipped_circumcenter]")
{
    using Scalar = double;
    const Eigen::Vector3<Scalar> p1(0, 0, 0);
    const Eigen::Vector3<Scalar> p2(1, 0, 0);
    const Eigen::Vector3<Scalar> p3(0.5, 1, 0);

    Scalar l1, l2, l3;
    auto c = lagrange::internal::clipped_triangle_circumcenter(p1, p2, p3, l1, l2, l3);

    REQUIRE(std::isfinite(c.x()));
    REQUIRE(std::isfinite(c.y()));
    REQUIRE(std::isfinite(c.z()));
    REQUIRE(l1 >= 0);
    REQUIRE(l2 >= 0);
    REQUIRE(l3 >= 0);
    REQUIRE(std::abs(l1 + l2 + l3 - 1) < 1e-6);
    // Expected circumcenter of this isoceles triangle: (0.5, 0.375, 0).
    REQUIRE(std::abs(c.x() - 0.5) < 1e-6);
    REQUIRE(std::abs(c.y() - 0.375) < 1e-6);
}

TEST_CASE(
    "clipped_triangle_circumcenter: degenerate (collinear) triangle falls back to barycenter",
    "[core][clipped_circumcenter]")
{
    using Scalar = double;
    // All three points on the x-axis: degenerate, |d| < denorm_min.
    const Eigen::Vector3<Scalar> p1(0, 0, 0);
    const Eigen::Vector3<Scalar> p2(1, 0, 0);
    const Eigen::Vector3<Scalar> p3(2, 0, 0);

    Scalar l1, l2, l3;
    auto c = lagrange::internal::clipped_triangle_circumcenter(p1, p2, p3, l1, l2, l3);

    REQUIRE(std::isfinite(c.x()));
    REQUIRE(std::isfinite(c.y()));
    REQUIRE(std::isfinite(c.z()));
    // Fallback: uniform barycentric coordinates.
    REQUIRE(std::abs(l1 - 1.0 / 3.0) < 1e-12);
    REQUIRE(std::abs(l2 - 1.0 / 3.0) < 1e-12);
    REQUIRE(std::abs(l3 - 1.0 / 3.0) < 1e-12);
    // Barycenter of (0,0,0), (1,0,0), (2,0,0) is (1, 0, 0).
    REQUIRE(std::abs(c.x() - 1.0) < 1e-12);
    REQUIRE(std::abs(c.y()) < 1e-12);
    REQUIRE(std::abs(c.z()) < 1e-12);
}

TEST_CASE(
    "clipped_triangle_circumcenter: coincident points fall back to barycenter",
    "[core][clipped_circumcenter]")
{
    using Scalar = double;
    // Three coincident points: |d| = 0.
    const Eigen::Vector3<Scalar> p(2, -1, 3);

    Scalar l1, l2, l3;
    auto c = lagrange::internal::clipped_triangle_circumcenter(p, p, p, l1, l2, l3);

    REQUIRE(std::isfinite(c.x()));
    REQUIRE(std::isfinite(c.y()));
    REQUIRE(std::isfinite(c.z()));
    REQUIRE(std::abs(l1 - 1.0 / 3.0) < 1e-12);
    REQUIRE(std::abs(l2 - 1.0 / 3.0) < 1e-12);
    REQUIRE(std::abs(l3 - 1.0 / 3.0) < 1e-12);
    REQUIRE((c - p).norm() < 1e-12);
}
