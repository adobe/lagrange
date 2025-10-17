/*
 * Copyright 2022 Adobe. All rights reserved.
 * This file is licensed to you under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License. You may obtain a copy
 * of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under
 * the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
 * OF ANY KIND, either express or implied. See the License for the specific language
 * governing permissions and limitations under the License.
 */
#include <lagrange/testing/common.h>
#include <catch2/benchmark/catch_benchmark.hpp>
#include <catch2/catch_approx.hpp>
#include <cmath>

#include <lagrange/Mesh.h>
#include <lagrange/common.h>
#include <lagrange/compute_weighted_corner_normal.h>
#include <lagrange/create_mesh.h>
#include <lagrange/mesh_convert.h>
#include <lagrange/views.h>

TEST_CASE("compute_weighted_corner_normal", "[surface][attribute][utilities][normal]")
{
    using namespace lagrange;
    using Scalar = double;
    using Index = uint32_t;

    SECTION("Cube")
    {
        lagrange::SurfaceMesh<Scalar, Index> mesh;
        mesh.add_vertex({0, 0, 0});
        mesh.add_vertex({1, 0, 0});
        mesh.add_vertex({0, 1, 0});
        mesh.add_vertex({1, 1, 0});
        mesh.add_vertex({0, 0, 1});
        mesh.add_vertex({1, 0, 1});
        mesh.add_vertex({0, 1, 1});
        mesh.add_vertex({1, 1, 1});
        mesh.add_quad(1, 0, 2, 3);
        mesh.add_quad(4, 5, 7, 6);
        mesh.add_quad(1, 3, 7, 5);
        mesh.add_quad(2, 0, 4, 6);
        mesh.add_quad(6, 7, 3, 2);
        mesh.add_quad(0, 1, 5, 4);

        auto id = compute_weighted_corner_normal(mesh);
        REQUIRE(mesh.is_attribute_type<Scalar>(id));
        REQUIRE(!mesh.is_attribute_indexed(id));

        auto normals = matrix_view(mesh.template get_attribute<Scalar>(id));
        REQUIRE(normals.rows() == 24);
        for (size_t i = 0; i < 24; i++) {
            REQUIRE(std::abs(normals.row(i).array().abs().maxCoeff()) == Catch::Approx(1));
        }
    }
    SECTION("tiny angles")
    {
        lagrange::SurfaceMesh<Scalar, Index> mesh;
        mesh.add_vertex({0, 0, 0});
        mesh.add_vertex({1.000000001, 1, 0});
        mesh.add_vertex({1, 1, 0});
        mesh.add_triangle(0, 1, 2);

        CornerNormalOptions options;
        SECTION("Uniform")
        {
            options.weight_type = NormalWeightingType::Uniform;
        }
        SECTION("CoernerTriangleArea")
        {
            options.weight_type = NormalWeightingType::CornerTriangleArea;
        }
        SECTION("Angle")
        {
            options.weight_type = NormalWeightingType::Angle;
        }

        auto id = compute_weighted_corner_normal(mesh, options);
        REQUIRE(mesh.is_attribute_type<Scalar>(id));
        REQUIRE(!mesh.is_attribute_indexed(id));

        auto normals = matrix_view(mesh.template get_attribute<Scalar>(id));
        REQUIRE(normals.allFinite());
        REQUIRE((normals.template leftCols<2>().array() == 0).all());
        REQUIRE((normals.col(2).array() > 0).all());
    }
}

TEST_CASE(
    "compute_weighted_corner_normal benchmark",
    "[surface][attribute][normal][utilities][!benchmark]")
{
    using namespace lagrange;
    using Scalar = double;
    using Index = uint32_t;

    auto mesh = lagrange::testing::load_surface_mesh<Scalar, Index>("open/core/dragon.obj");

    BENCHMARK_ADVANCED("compute_weighted_corner_normal")(Catch::Benchmark::Chronometer meter)
    {
        if (mesh.has_attribute("@corner_normal"))
            mesh.delete_attribute("@corner_normal", AttributeDeletePolicy::Force);
        meter.measure([&]() { return compute_weighted_corner_normal(mesh); });
    };
}
