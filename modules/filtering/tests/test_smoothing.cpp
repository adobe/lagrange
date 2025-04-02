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

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <Include/PreProcessor.h>
#include <Include/GradientDomain.h>
#include <Include/CurvatureMetric.h>
#include <Misha/Geometry.h>
#include <Misha/Miscellany.h>
#include <lagrange/utils/warnon.h>
// clang-format on

#include <catch2/catch_test_macros.hpp>
////////////////////////////////////////////////////////////////////////////////

namespace {

using Real = double;
static const unsigned int Dim = 3;
static const unsigned int K = 2;

using namespace lagrange::filtering;
using Point = MishaK::Geometry::Point<Real, Dim>;

double compare_vertices(const std::vector<Point>& v1, const std::vector<Point>& v2)
{
    REQUIRE(v1.size() == v2.size());
    double e = 0;
    for (size_t i = 0; i < v1.size(); i++)
        e = std::max<double>(e, Point::SquareNorm(v1[i] - v2[i]));
    return sqrt(e);
}

std::pair<Point, Real> center_and_radius(const std::vector<Point>& v)
{
    Point center;
    for (size_t i = 0; i < v.size(); i++) center += v[i];
    center /= v.size();

    Real radius = 0;
    for (size_t i = 0; i < v.size(); i++) radius += Point::SquareNorm(v[i] - center);
    radius = sqrt(radius / v.size());

    return std::make_pair(center, radius);
}

double spherical_deviation(const std::vector<Point>& v)
{
    auto c_and_r = center_and_radius(v);
    double s = 0;
    for (size_t i = 0; i < v.size(); i++) {
        double l = Point::Length((v[i] - c_and_r.first) / c_and_r.second);
        s += (l - 1.) * (l - 1.);
    }
    return sqrt(s / v.size());
}

void normalize(std::vector<Point>& v, std::pair<Point, Real> c_and_r)
{
    for (size_t i = 0; i < v.size(); i++) v[i] = (v[i] - c_and_r.first) / c_and_r.second;
}

void normalize(std::vector<Point>& v)
{
    normalize(v, center_and_radius(v));
}

template <typename Scalar, typename Index>
std::vector<Point> get_vertices(const lagrange::SurfaceMesh<Scalar, Index>& t_mesh)
{
    std::vector<Point> vertices(t_mesh.get_num_vertices());

    // Retrieve input vertex buffer
    auto& input_coords = t_mesh.get_vertex_to_position();
    la_runtime_assert(
        input_coords.get_num_elements() == t_mesh.get_num_vertices(),
        "Number of vertices should match number of vertices");


    auto _input_coords = input_coords.get_all();

    for (unsigned int i = 0; i < t_mesh.get_num_vertices(); i++) {
        for (unsigned int k = 0; k <= K; k++) {
            vertices[i][k] = static_cast<Real>(_input_coords[i * (K + 1) + k]);
        }
    }

    return vertices;
}

template <typename Scalar, typename Index>
void set_vertices(lagrange::SurfaceMesh<Scalar, Index>& t_mesh, const std::vector<Point>& vertices)
{
    REQUIRE(vertices.size() == t_mesh.get_num_vertices());

    auto v = lagrange::vertex_ref(t_mesh);
    for (size_t i = 0; i < t_mesh.get_num_vertices(); i++)
        for (unsigned int d = 0; d < Dim; d++) v(i, d) = static_cast<Scalar>(vertices[i][d]);
}

} // namespace

TEST_CASE("Mesh Smoothing", "[filtering]")
{
    using Scalar = float;
    using Index = uint32_t;

    lagrange::filtering::SmoothingOptions smoothing_options;

    // Check that multiple runs give similar results
    {
        auto mesh1 = lagrange::testing::load_surface_mesh<Scalar, Index>("open/core/dragon.obj");
        auto mesh2 = lagrange::testing::load_surface_mesh<Scalar, Index>("open/core/dragon.obj");

        lagrange::filtering::mesh_smoothing(mesh1, smoothing_options);
        lagrange::filtering::mesh_smoothing(mesh2, smoothing_options);

        // [WARNING] The vertex positions across the two runs are close, but not the same
        {
            auto v1 = get_vertices(mesh1);
            auto v2 = get_vertices(mesh2);
            REQUIRE(compare_vertices(v1, v1) < 1e-10);
        }
        REQUIRE(facet_view(mesh1) == facet_view(mesh2));
        // REQUIRE(vertex_view(mesh1) == vertex_view(mesh2));
    }

    // Check that a noisy sphere becomes less noisy
    {
        std::mt19937 gen;
        std::uniform_real_distribution<> dis(-1.0, 1.0);

        auto RandomPoint = [&](void) {
            while (true) {
                Point p;
                for (unsigned int d = 0; d < Dim; d++) p[d] = dis(gen);
                if (Point::SquareNorm(p) <= 1) return p;
            }
            return Point();
        };

        auto mesh = lagrange::testing::load_surface_mesh<Scalar, Index>("open/core/ball.obj");

        auto v_pre = get_vertices(mesh);
        normalize(v_pre);
        for (size_t i = 0; i < v_pre.size(); i++) v_pre[i] += RandomPoint() * 1e-2;
        set_vertices(mesh, v_pre);

        lagrange::filtering::mesh_smoothing(mesh, smoothing_options);

        auto v_post = get_vertices(mesh);

        REQUIRE(spherical_deviation(v_post) < spherical_deviation(v_pre));
    }
}
