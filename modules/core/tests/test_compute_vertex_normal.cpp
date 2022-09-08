/*
 * Copyright 2017 Adobe. All rights reserved.
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
#include <lagrange/compute_vertex_normal.h>
#include <lagrange/create_mesh.h>
#include <lagrange/mesh_convert.h>
#include <lagrange/views.h>
#include <lagrange/utils/geometry3d.h>

TEST_CASE("compute_vertex_normal", "[surface][attribute][normal][utilities]")
{
    using namespace lagrange;
    using Scalar = double;
    using Index = uint32_t;

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

    auto id = compute_vertex_normal(mesh);
    REQUIRE(mesh.is_attribute_type<Scalar>(id));
    REQUIRE(!mesh.is_attribute_indexed(id));

    auto normals = matrix_view(mesh.template get_attribute<Scalar>(id));
    for (size_t i = 0; i < 8; i++) {
        REQUIRE(std::abs(normals(i, 0)) == Catch::Approx(1.0 / sqrt(3.0)));
        REQUIRE(std::abs(normals(i, 1)) == Catch::Approx(1.0 / sqrt(3.0)));
        REQUIRE(std::abs(normals(i, 2)) == Catch::Approx(1.0 / sqrt(3.0)));
    }
}

TEST_CASE("compute_vertex_normal Waffle", "[surface][attribute][normal][utilities]" LA_CORP_FLAG)
{
    using namespace lagrange;
    using Scalar = double;
    using Index = uint32_t;

    auto mesh = lagrange::testing::load_surface_mesh<Scalar, Index>("corp/core/WaffleSkin.obj");
    auto id = lagrange::compute_vertex_normal(mesh);
    auto normals = matrix_view(mesh.template get_attribute<Scalar>(id));
    REQUIRE(normals.allFinite());

#ifdef LAGRANGE_ENABLE_LEGACY_FUNCTIONS
    using MeshType = TriangleMesh3D;
    auto legacy_mesh = to_legacy_mesh<MeshType>(mesh);
    lagrange::compute_vertex_normal(*legacy_mesh);
    REQUIRE(legacy_mesh->has_vertex_attribute("normal"));
    const auto& vertex_normals = legacy_mesh->get_vertex_attribute("normal");
    REQUIRE(vertex_normals.allFinite());
#endif
}

TEST_CASE("compute_vertex_normal benchmark", "[surface][attribute][normal][utilities][!benchmark]")
{
    using namespace lagrange;
    using Scalar = double;
    using Index = uint32_t;

    auto mesh = lagrange::testing::load_surface_mesh<Scalar, Index>("open/core/dragon.obj");

    BENCHMARK_ADVANCED("compute_vertex_normal")(Catch::Benchmark::Chronometer meter)
    {
        if (mesh.has_attribute("@vertex_normal"))
            mesh.delete_attribute("@vertex_normal", AttributeDeletePolicy::Force);
        meter.measure([&]() { return compute_vertex_normal(mesh); });
    };

#ifdef LAGRANGE_ENABLE_LEGACY_FUNCTIONS
    using MeshType = TriangleMesh3D;
    auto legacy_mesh = to_legacy_mesh<MeshType>(mesh);

    BENCHMARK_ADVANCED("legacy::compute_vertex_normal")(Catch::Benchmark::Chronometer meter)
    {
        if (legacy_mesh->has_vertex_attribute("normal"))
            legacy_mesh->remove_vertex_attribute("normal");
        meter.measure([&]() { return compute_vertex_normal(*legacy_mesh); });
    };
#endif
}

#ifdef LAGRANGE_ENABLE_LEGACY_FUNCTIONS
TEST_CASE(
    "legacy::compute_vertex_normal vs compute_vertex_normal",
    "[mesh][attribute][normal][legacy]")
{
    using namespace lagrange;
    using Scalar = double;
    using Index = uint32_t;
    using MeshType = TriangleMesh3D;

    auto mesh = lagrange::testing::load_surface_mesh<Scalar, Index>("open/core/blub/blub.obj");
    auto id = compute_vertex_normal(mesh);
    auto new_normals = matrix_view(mesh.template get_attribute<Scalar>(id));

    using MeshType = TriangleMesh3D;
    auto legacy_mesh = to_legacy_mesh<MeshType>(mesh);
    CHECK(!legacy_mesh->has_vertex_attribute("normal"));
    compute_vertex_normal(*legacy_mesh);
    CHECK(legacy_mesh->has_vertex_attribute("normal"));

    auto new_mesh = to_surface_mesh_copy<Scalar, Index>(*legacy_mesh);
    auto old_normals = matrix_view(new_mesh.template get_attribute<Scalar>("normal"));

    const auto num_vertices = mesh.get_num_vertices();
    for (auto i : range(num_vertices)) {
        const Eigen::Matrix<Scalar, 1, 3> new_normal = new_normals.row(i);
        const Eigen::Matrix<Scalar, 1, 3> old_normal = old_normals.row(i);
        INFO("new_normal=(" << new_normal << ")");
        INFO("old_normal=(" << old_normal << ")");
        REQUIRE(angle_between(new_normal, old_normal) == Catch::Approx(0).margin(1e-3));
    }
}

TEST_CASE("legacy::compute_vertex_normal", "[mesh][triangle][attribute][vertex_normal]")
{
    using namespace lagrange;
    auto mesh = create_cube();
    const size_t num_vertices = mesh->get_num_vertices();
    compute_vertex_normal(*mesh);
    REQUIRE(mesh->has_vertex_attribute("normal"));

    const auto& vertex_normals = mesh->get_vertex_attribute("normal");
    REQUIRE(safe_cast<size_t>(vertex_normals.rows()) == num_vertices);
    REQUIRE(vertex_normals.cols() == 3);

    for (size_t i = 0; i < num_vertices; i++) {
        REQUIRE(std::abs(vertex_normals(i, 0)) == Catch::Approx(1.0 / sqrt(3.0)));
        REQUIRE(std::abs(vertex_normals(i, 1)) == Catch::Approx(1.0 / sqrt(3.0)));
        REQUIRE(std::abs(vertex_normals(i, 2)) == Catch::Approx(1.0 / sqrt(3.0)));
    }
}

#endif
