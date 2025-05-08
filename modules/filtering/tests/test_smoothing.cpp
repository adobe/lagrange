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
////////////////////////////////////////////////////////////////////////////////
#include <random>

#include <lagrange/Attribute.h>
#include <lagrange/Logger.h>
#include <lagrange/SurfaceMeshTypes.h>
#include <lagrange/filtering/mesh_smoothing.h>
#include <lagrange/testing/common.h>
#include <lagrange/utils/assert.h>
#include <lagrange/views.h>

#include <tbb/parallel_for.h>

#include <catch2/catch_test_macros.hpp>
////////////////////////////////////////////////////////////////////////////////

namespace {

template<typename Derived>
auto center_and_radius(const Eigen::MatrixBase<Derived>& v)
{
    using Scalar = typename Derived::Scalar;
    Eigen::RowVector3<Scalar> center = v.colwise().mean();

    Scalar radius = std::sqrt((v.rowwise() - center).rowwise().squaredNorm().mean());

    return std::make_pair(center, radius);
}

template<typename Derived>
double spherical_deviation(const Eigen::MatrixBase<Derived>& v)
{
    auto [center, radius] = center_and_radius(v);
    double s = 0;
    for (Eigen::Index i = 0; i < v.rows(); ++i) {
        double l = (v.row(i) - center).norm() / radius;
        s += (l - 1.) * (l - 1.);
    }
    return std::sqrt(s / v.rows());
}

template<typename Derived>
void normalize(Eigen::MatrixBase<Derived>& v)
{
    auto [center, radius] = center_and_radius(v);
    v.rowwise() -= center;
    v /= radius;
}

} // namespace

TEST_CASE("Mesh Smoothing", "[filtering]")
{
    using Scalar = float;
    using Index = uint32_t;

    lagrange::filtering::SmoothingOptions smoothing_options;

    // Check that multiple runs give similar results
    {
        auto mesh1 = lagrange::testing::load_surface_mesh<Scalar, Index>("open/core/bunny_simple.obj");
        auto mesh2 = mesh1;

        lagrange::filtering::mesh_smoothing(mesh1, smoothing_options);
        lagrange::filtering::mesh_smoothing(mesh2, smoothing_options);

        // [WARNING] The vertex positions across the two runs are close, but not the same
        {
            auto v1 = vertex_view(mesh1);
            auto v2 = vertex_view(mesh2);
            REQUIRE((v1 - v2).lpNorm<Eigen::Infinity>() < 1e-8);
        }
        REQUIRE(facet_view(mesh1) == facet_view(mesh2));
        // REQUIRE(vertex_view(mesh1) == vertex_view(mesh2));
    }

    // Check that a noisy sphere becomes less noisy
    {
        std::mt19937 gen;
        std::uniform_real_distribution<> dis(-1.0, 1.0);

        auto random_point = [&](void) {
            while (true) {
                Eigen::Vector3f p(dis(gen), dis(gen), dis(gen));
                if (p.squaredNorm() <= 1) return p;
            }
            return Eigen::Vector3f();
        };

        auto mesh = lagrange::testing::load_surface_mesh<Scalar, Index>("open/core/ball.obj");

        lagrange::RowMatrix<float> v_pre = vertex_view(mesh);
        normalize(v_pre);
        for (Eigen::Index i = 0; i < v_pre.rows(); ++i) {
            v_pre.row(i) += random_point() * 1e-2;
        }
        vertex_ref(mesh) = v_pre;

        lagrange::filtering::mesh_smoothing(mesh, smoothing_options);

        auto v_post = vertex_view(mesh);

        REQUIRE(spherical_deviation(v_post) < spherical_deviation(v_pre));
    }
}
