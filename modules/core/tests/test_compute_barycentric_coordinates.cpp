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
#include <lagrange/compute_barycentric_coordinates.h>
#include <lagrange/testing/common.h>

#include <catch2/benchmark/catch_benchmark.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

namespace {

using Catch::Matchers::WithinAbs;
constexpr double eps = 1e-10;

/// Verify that barycentric coordinates are valid:
/// - Sum to 1
/// - Reconstruct the query point
template <typename Scalar, int Dim>
void verify_barycentric(
    const Eigen::Matrix<Scalar, Dim, 1>& v0,
    const Eigen::Matrix<Scalar, Dim, 1>& v1,
    const Eigen::Matrix<Scalar, Dim, 1>& v2,
    const Eigen::Matrix<Scalar, Dim, 1>& p,
    const Eigen::Matrix<Scalar, 3, 1>& bary)
{
    // Barycentric coordinates must sum to 1.
    REQUIRE_THAT(static_cast<double>(bary.sum()), WithinAbs(1.0, eps));

    // Reconstruct point from barycentric coordinates.
    auto reconstructed = bary(0) * v0 + bary(1) * v1 + bary(2) * v2;
    for (int d = 0; d < Dim; ++d) {
        REQUIRE_THAT(
            static_cast<double>(reconstructed(d)),
            WithinAbs(static_cast<double>(p(d)), eps));
    }
}

} // namespace

TEST_CASE("compute_barycentric_coordinates: vertices at origin", "[core][barycentric]")
{
    // Triangle with a vertex at the origin — the old implementation fails here because the
    // matrix [v0|v1|v2] has a zero column, making it singular.
    Eigen::Vector3d v0(0, 0, 0);
    Eigen::Vector3d v1(1, 0, 0);
    Eigen::Vector3d v2(0, 1, 0);

    SECTION("query at v0")
    {
        auto bary = lagrange::compute_barycentric_coordinates(v0, v1, v2, v0);
        verify_barycentric(v0, v1, v2, v0, bary);
        REQUIRE_THAT(bary(0), WithinAbs(1.0, eps));
        REQUIRE_THAT(bary(1), WithinAbs(0.0, eps));
        REQUIRE_THAT(bary(2), WithinAbs(0.0, eps));
    }

    SECTION("query at v1")
    {
        auto bary = lagrange::compute_barycentric_coordinates(v0, v1, v2, v1);
        verify_barycentric(v0, v1, v2, v1, bary);
        REQUIRE_THAT(bary(0), WithinAbs(0.0, eps));
        REQUIRE_THAT(bary(1), WithinAbs(1.0, eps));
        REQUIRE_THAT(bary(2), WithinAbs(0.0, eps));
    }

    SECTION("query at centroid")
    {
        Eigen::Vector3d p = (v0 + v1 + v2) / 3.0;
        auto bary = lagrange::compute_barycentric_coordinates(v0, v1, v2, p);
        verify_barycentric(v0, v1, v2, p, bary);
        REQUIRE_THAT(bary(0), WithinAbs(1.0 / 3.0, eps));
        REQUIRE_THAT(bary(1), WithinAbs(1.0 / 3.0, eps));
        REQUIRE_THAT(bary(2), WithinAbs(1.0 / 3.0, eps));
    }
}

TEST_CASE("compute_barycentric_coordinates: coplanar z=0", "[core][barycentric]")
{
    // All vertices in the z=0 plane — the old implementation builds a 3x3 matrix with all-zero
    // third row, making it singular.
    Eigen::Vector3d v0(1, 0, 0);
    Eigen::Vector3d v1(3, 0, 0);
    Eigen::Vector3d v2(2, 2, 0);

    SECTION("query at centroid")
    {
        Eigen::Vector3d p = (v0 + v1 + v2) / 3.0;
        auto bary = lagrange::compute_barycentric_coordinates(v0, v1, v2, p);
        verify_barycentric(v0, v1, v2, p, bary);
    }

    SECTION("query at v0")
    {
        auto bary = lagrange::compute_barycentric_coordinates(v0, v1, v2, v0);
        verify_barycentric(v0, v1, v2, v0, bary);
        REQUIRE_THAT(bary(0), WithinAbs(1.0, eps));
    }

    SECTION("query at midpoint of edge v0-v1")
    {
        Eigen::Vector3d p = (v0 + v1) / 2.0;
        auto bary = lagrange::compute_barycentric_coordinates(v0, v1, v2, p);
        verify_barycentric(v0, v1, v2, p, bary);
        REQUIRE_THAT(bary(0), WithinAbs(0.5, eps));
        REQUIRE_THAT(bary(1), WithinAbs(0.5, eps));
        REQUIRE_THAT(bary(2), WithinAbs(0.0, eps));
    }
}

TEST_CASE("compute_barycentric_coordinates: coplanar y=0", "[core][barycentric]")
{
    Eigen::Vector3d v0(0, 0, 0);
    Eigen::Vector3d v1(4, 0, 0);
    Eigen::Vector3d v2(2, 0, 3);
    Eigen::Vector3d p = (v0 + v1 + v2) / 3.0;

    auto bary = lagrange::compute_barycentric_coordinates(v0, v1, v2, p);
    verify_barycentric(v0, v1, v2, p, bary);
}

TEST_CASE("compute_barycentric_coordinates: non-axis-aligned plane", "[core][barycentric]")
{
    // Triangle in an arbitrary plane not aligned with any axis.
    Eigen::Vector3d v0(1, 2, 3);
    Eigen::Vector3d v1(4, 2, 3);
    Eigen::Vector3d v2(2.5, 5, 3);

    SECTION("query at centroid")
    {
        Eigen::Vector3d p = (v0 + v1 + v2) / 3.0;
        auto bary = lagrange::compute_barycentric_coordinates(v0, v1, v2, p);
        verify_barycentric(v0, v1, v2, p, bary);
    }

    SECTION("query at 0.2/0.3/0.5 blend")
    {
        Eigen::Vector3d p = 0.2 * v0 + 0.3 * v1 + 0.5 * v2;
        auto bary = lagrange::compute_barycentric_coordinates(v0, v1, v2, p);
        verify_barycentric(v0, v1, v2, p, bary);
        REQUIRE_THAT(bary(0), WithinAbs(0.2, eps));
        REQUIRE_THAT(bary(1), WithinAbs(0.3, eps));
        REQUIRE_THAT(bary(2), WithinAbs(0.5, eps));
    }
}

TEST_CASE("compute_barycentric_coordinates: general 3D triangle", "[core][barycentric]")
{
    // Triangle that is NOT in any axis-aligned plane (this case already worked).
    Eigen::Vector3d v0(1, 0, 0);
    Eigen::Vector3d v1(0, 1, 0);
    Eigen::Vector3d v2(0, 0, 1);

    SECTION("query at centroid")
    {
        Eigen::Vector3d p = (v0 + v1 + v2) / 3.0;
        auto bary = lagrange::compute_barycentric_coordinates(v0, v1, v2, p);
        verify_barycentric(v0, v1, v2, p, bary);
    }

    SECTION("query at vertices")
    {
        for (int k = 0; k < 3; ++k) {
            const auto& vk = (k == 0) ? v0 : (k == 1) ? v1 : v2;
            auto bary = lagrange::compute_barycentric_coordinates(v0, v1, v2, vk);
            verify_barycentric(v0, v1, v2, vk, bary);
            REQUIRE_THAT(bary(k), WithinAbs(1.0, eps));
            REQUIRE_THAT(bary((k + 1) % 3), WithinAbs(0.0, eps));
            REQUIRE_THAT(bary((k + 2) % 3), WithinAbs(0.0, eps));
        }
    }
}

TEST_CASE("compute_barycentric_coordinates: 2D triangle", "[core][barycentric]")
{
    // 2D points (common in UV space).
    Eigen::Vector2d v0(0, 0);
    Eigen::Vector2d v1(1, 0);
    Eigen::Vector2d v2(0.5, 1);

    SECTION("query at centroid")
    {
        Eigen::Vector2d p = (v0 + v1 + v2) / 3.0;
        auto bary = lagrange::compute_barycentric_coordinates(v0, v1, v2, p);
        verify_barycentric(v0, v1, v2, p, bary);
    }

    SECTION("query at v2")
    {
        auto bary = lagrange::compute_barycentric_coordinates(v0, v1, v2, v2);
        verify_barycentric(v0, v1, v2, v2, bary);
        REQUIRE_THAT(bary(2), WithinAbs(1.0, eps));
    }
}

TEST_CASE("compute_barycentric_coordinates: collinear vertices", "[core][barycentric]")
{
    // Degenerate triangle: all three vertices are collinear along the x-axis.
    // The least-squares solution should still return coordinates that sum to 1
    // and reconstruct p as well as possible (exact when p is on the line).
    Eigen::Vector3d v0(0, 0, 0);
    Eigen::Vector3d v1(1, 0, 0);
    Eigen::Vector3d v2(2, 0, 0);

    SECTION("query at v0")
    {
        auto bary = lagrange::compute_barycentric_coordinates(v0, v1, v2, v0);
        REQUIRE_THAT(static_cast<double>(bary.sum()), WithinAbs(1.0, eps));
        auto reconstructed = bary(0) * v0 + bary(1) * v1 + bary(2) * v2;
        for (int d = 0; d < 3; ++d) {
            REQUIRE_THAT(
                static_cast<double>(reconstructed(d)),
                WithinAbs(static_cast<double>(v0(d)), eps));
        }
    }

    SECTION("query at midpoint of v0-v2")
    {
        Eigen::Vector3d p = (v0 + v2) / 2.0; // same as v1
        auto bary = lagrange::compute_barycentric_coordinates(v0, v1, v2, p);
        REQUIRE_THAT(static_cast<double>(bary.sum()), WithinAbs(1.0, eps));
        auto reconstructed = bary(0) * v0 + bary(1) * v1 + bary(2) * v2;
        for (int d = 0; d < 3; ++d) {
            REQUIRE_THAT(
                static_cast<double>(reconstructed(d)),
                WithinAbs(static_cast<double>(p(d)), eps));
        }
    }

    SECTION("query off the line — best approximation")
    {
        Eigen::Vector3d p(0.5, 1.0, 0.0); // off the line
        auto bary = lagrange::compute_barycentric_coordinates(v0, v1, v2, p);
        // Coordinates should still sum to 1.
        REQUIRE_THAT(static_cast<double>(bary.sum()), WithinAbs(1.0, eps));
        // Reconstruction won't be exact (p is not on the line), but the x-component should match.
        auto reconstructed = bary(0) * v0 + bary(1) * v1 + bary(2) * v2;
        REQUIRE_THAT(static_cast<double>(reconstructed(0)), WithinAbs(0.5, eps));
    }
}

TEST_CASE("compute_barycentric_coordinates: float precision", "[core][barycentric]")
{
    Eigen::Vector3f v0(0.0f, 0.0f, 0.0f);
    Eigen::Vector3f v1(1.0f, 0.0f, 0.0f);
    Eigen::Vector3f v2(0.0f, 1.0f, 0.0f);
    Eigen::Vector3f p = (v0 + v1 + v2) / 3.0f;

    auto bary = lagrange::compute_barycentric_coordinates(v0, v1, v2, p);
    REQUIRE_THAT(static_cast<double>(bary.sum()), WithinAbs(1.0, 1e-5));
    auto reconstructed = bary(0) * v0 + bary(1) * v1 + bary(2) * v2;
    for (int d = 0; d < 3; ++d) {
        REQUIRE_THAT(
            static_cast<double>(reconstructed(d)),
            WithinAbs(static_cast<double>(p(d)), 1e-5));
    }
}

TEST_CASE("compute_barycentric_coordinates: fully dynamic VectorXd", "[core][barycentric]")
{
    // Fully dynamic vectors (VectorXd) should work without heap allocation inside the function.
    SECTION("3D")
    {
        Eigen::VectorXd v0(3), v1(3), v2(3), p(3);
        v0 << 0, 0, 0;
        v1 << 1, 0, 0;
        v2 << 0, 1, 0;
        p << 0.25, 0.25, 0;
        auto bary = lagrange::compute_barycentric_coordinates(v0, v1, v2, p);
        REQUIRE_THAT(static_cast<double>(bary.sum()), WithinAbs(1.0, eps));
        Eigen::VectorXd reconstructed = bary(0) * v0 + bary(1) * v1 + bary(2) * v2;
        for (int d = 0; d < 3; ++d) {
            REQUIRE_THAT(
                static_cast<double>(reconstructed(d)),
                WithinAbs(static_cast<double>(p(d)), eps));
        }
    }

    SECTION("2D")
    {
        Eigen::VectorXd v0(2), v1(2), v2(2), p(2);
        v0 << 0, 0;
        v1 << 1, 0;
        v2 << 0.5, 1;
        p = (v0 + v1 + v2) / 3.0;
        auto bary = lagrange::compute_barycentric_coordinates(v0, v1, v2, p);
        REQUIRE_THAT(static_cast<double>(bary.sum()), WithinAbs(1.0, eps));
        Eigen::VectorXd reconstructed = bary(0) * v0 + bary(1) * v1 + bary(2) * v2;
        for (int d = 0; d < 2; ++d) {
            REQUIRE_THAT(
                static_cast<double>(reconstructed(d)),
                WithinAbs(static_cast<double>(p(d)), eps));
        }
    }

    SECTION("coplanar z=0")
    {
        Eigen::VectorXd v0(3), v1(3), v2(3), p(3);
        v0 << 1, 0, 0;
        v1 << 3, 0, 0;
        v2 << 2, 2, 0;
        p = (v0 + v1 + v2) / 3.0;
        auto bary = lagrange::compute_barycentric_coordinates(v0, v1, v2, p);
        REQUIRE_THAT(static_cast<double>(bary.sum()), WithinAbs(1.0, eps));
        Eigen::VectorXd reconstructed = bary(0) * v0 + bary(1) * v1 + bary(2) * v2;
        for (int d = 0; d < 3; ++d) {
            REQUIRE_THAT(
                static_cast<double>(reconstructed(d)),
                WithinAbs(static_cast<double>(p(d)), eps));
        }
    }
}

// ---- Benchmarks ----

namespace {

/// Fast-path only: 2x2 Gram matrix solve (no degeneracy check, no fallback).
template <typename PointType>
auto bary_fast_path_only(
    const Eigen::MatrixBase<PointType>& v0,
    const Eigen::MatrixBase<PointType>& v1,
    const Eigen::MatrixBase<PointType>& v2,
    const Eigen::MatrixBase<PointType>& p) -> Eigen::Matrix<typename PointType::Scalar, 3, 1>
{
    using Scalar = typename PointType::Scalar;
    const auto e1 = (v1 - v0).eval();
    const auto e2 = (v2 - v0).eval();
    const auto ep = (p - v0).eval();

    Eigen::Matrix<Scalar, 2, 2> G;
    G(0, 0) = e1.squaredNorm();
    G(0, 1) = e1.dot(e2);
    G(1, 0) = G(0, 1);
    G(1, 1) = e2.squaredNorm();

    const Eigen::Matrix<Scalar, 2, 1> b(ep.dot(e1), ep.dot(e2));
    const Eigen::Matrix<Scalar, 2, 1> uv = G.inverse() * b;

    Eigen::Matrix<Scalar, 3, 1> bary;
    bary(0) = Scalar(1) - uv(0) - uv(1);
    bary(1) = uv(0);
    bary(2) = uv(1);
    return bary;
}

/// Slow-path only: constrained least-squares QR solve.
template <typename PointType>
auto bary_slow_path_only(
    const Eigen::MatrixBase<PointType>& v0,
    const Eigen::MatrixBase<PointType>& v1,
    const Eigen::MatrixBase<PointType>& v2,
    const Eigen::MatrixBase<PointType>& p) -> Eigen::Matrix<typename PointType::Scalar, 3, 1>
{
    using Scalar = typename PointType::Scalar;
    constexpr int Dim = PointType::SizeAtCompileTime;
    constexpr int MaxDim = PointType::MaxSizeAtCompileTime;
    constexpr int EffectiveMaxDim = (MaxDim != Eigen::Dynamic) ? MaxDim : 3;
    constexpr int Rows = (Dim != Eigen::Dynamic) ? Dim + 1 : Eigen::Dynamic;
    constexpr int MaxRows = EffectiveMaxDim + 1;

    const Eigen::Index dim = p.size();
    Eigen::Matrix<Scalar, Rows, 3, 0, MaxRows, 3> M(dim + 1, 3);
    M.row(dim).setOnes();
    for (Eigen::Index d = 0; d < dim; ++d) {
        M(d, 0) = v0[d];
        M(d, 1) = v1[d];
        M(d, 2) = v2[d];
    }

    Eigen::Matrix<Scalar, Rows, 1, 0, MaxRows, 1> rhs(dim + 1);
    for (Eigen::Index d = 0; d < dim; ++d) {
        rhs(d) = p[d];
    }
    rhs(dim) = Scalar(1);

    return M.colPivHouseholderQr().solve(rhs);
}

} // namespace

TEST_CASE("compute_barycentric_coordinates: benchmark", "[core][barycentric][!benchmark]")
{
    // -- Non-degenerate triangles --

    // 2D fixed-size
    Eigen::Vector2d v0_2d(0, 0), v1_2d(1, 0), v2_2d(0.5, 1);
    Eigen::Vector2d p_2d = 0.2 * v0_2d + 0.3 * v1_2d + 0.5 * v2_2d;

    // 3D fixed-size
    Eigen::Vector3d v0_3d(1, 2, 3), v1_3d(4, 2, 3), v2_3d(2.5, 5, 3);
    Eigen::Vector3d p_3d = 0.2 * v0_3d + 0.3 * v1_3d + 0.5 * v2_3d;

    // 3D dynamic (VectorXd)
    Eigen::VectorXd v0_dyn(3), v1_dyn(3), v2_dyn(3), p_dyn(3);
    v0_dyn << 1, 2, 3;
    v1_dyn << 4, 2, 3;
    v2_dyn << 2.5, 5, 3;
    p_dyn = 0.2 * v0_dyn + 0.3 * v1_dyn + 0.5 * v2_dyn;

    // -- Degenerate triangles (collinear vertices, forces slow path) --

    // 2D degenerate
    Eigen::Vector2d dv0_2d(0, 0), dv1_2d(1, 0), dv2_2d(2, 0);
    Eigen::Vector2d dp_2d(0.5, 0);

    // 3D degenerate
    Eigen::Vector3d dv0_3d(0, 0, 0), dv1_3d(1, 0, 0), dv2_3d(2, 0, 0);
    Eigen::Vector3d dp_3d(0.5, 0, 0);

    // 3D degenerate dynamic
    Eigen::VectorXd dv0_dyn(3), dv1_dyn(3), dv2_dyn(3), dp_dyn(3);
    dv0_dyn << 0, 0, 0;
    dv1_dyn << 1, 0, 0;
    dv2_dyn << 2, 0, 0;
    dp_dyn << 0.5, 0, 0;

    // -- 2D benchmarks --

    BENCHMARK("2D fixed: fast-path only")
    {
        return bary_fast_path_only(v0_2d, v1_2d, v2_2d, p_2d);
    };

    BENCHMARK("2D fixed: slow-path only")
    {
        return bary_slow_path_only(v0_2d, v1_2d, v2_2d, p_2d);
    };

    BENCHMARK("2D fixed: combined (non-degenerate)")
    {
        return lagrange::compute_barycentric_coordinates(v0_2d, v1_2d, v2_2d, p_2d);
    };

    BENCHMARK("2D fixed: combined (degenerate)")
    {
        return lagrange::compute_barycentric_coordinates(dv0_2d, dv1_2d, dv2_2d, dp_2d);
    };

    // -- 3D fixed benchmarks --

    BENCHMARK("3D fixed: fast-path only")
    {
        return bary_fast_path_only(v0_3d, v1_3d, v2_3d, p_3d);
    };

    BENCHMARK("3D fixed: slow-path only")
    {
        return bary_slow_path_only(v0_3d, v1_3d, v2_3d, p_3d);
    };

    BENCHMARK("3D fixed: combined (non-degenerate)")
    {
        return lagrange::compute_barycentric_coordinates(v0_3d, v1_3d, v2_3d, p_3d);
    };

    BENCHMARK("3D fixed: combined (degenerate)")
    {
        return lagrange::compute_barycentric_coordinates(dv0_3d, dv1_3d, dv2_3d, dp_3d);
    };

    // -- 3D dynamic benchmarks --

    BENCHMARK("3D dynamic: fast-path only")
    {
        return bary_fast_path_only(v0_dyn, v1_dyn, v2_dyn, p_dyn);
    };

    BENCHMARK("3D dynamic: slow-path only")
    {
        return bary_slow_path_only(v0_dyn, v1_dyn, v2_dyn, p_dyn);
    };

    BENCHMARK("3D dynamic: combined (non-degenerate)")
    {
        return lagrange::compute_barycentric_coordinates(v0_dyn, v1_dyn, v2_dyn, p_dyn);
    };

    BENCHMARK("3D dynamic: combined (degenerate)")
    {
        return lagrange::compute_barycentric_coordinates(dv0_dyn, dv1_dyn, dv2_dyn, dp_dyn);
    };
}
