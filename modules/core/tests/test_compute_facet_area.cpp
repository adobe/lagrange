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
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <lagrange/Mesh.h>
#include <lagrange/common.h>
#include <lagrange/compute_area.h>
#include <lagrange/compute_facet_area.h>
#include <lagrange/create_mesh.h>
#include <lagrange/mesh_convert.h>
#include <lagrange/views.h>

TEST_CASE("compute_facet_area", "[core][area][surface]")
{
    using namespace lagrange;
    using Scalar = double;
    using Index = uint32_t;

    constexpr Scalar eps = std::numeric_limits<Scalar>::epsilon();
    FacetAreaOptions options;

    SECTION("2D triangle")
    {
        SurfaceMesh<Scalar, Index> mesh(2);
        mesh.add_vertex({0, 0});
        mesh.add_vertex({1, 0});
        mesh.add_vertex({0, 1});
        mesh.add_vertex({1, 1});
        mesh.add_triangle(0, 1, 2);
        mesh.add_triangle(2, 1, 3);

        auto id = compute_facet_area(mesh, options);
        REQUIRE(mesh.template is_attribute_type<Scalar>(id));
        REQUIRE(!mesh.is_attribute_indexed(id));
        REQUIRE(mesh.get_attribute_name(id) == options.output_attribute_name);

        const auto& areas = vector_view(mesh.get_attribute<Scalar>(id));
        REQUIRE_THAT(areas[0], Catch::Matchers::WithinAbs(0.5, eps));
        REQUIRE_THAT(areas[1], Catch::Matchers::WithinAbs(0.5, eps));
    }

    SECTION("2D quad")
    {
        SurfaceMesh<Scalar, Index> mesh(2);
        mesh.add_vertex({0, 0});
        mesh.add_vertex({1, 0});
        mesh.add_vertex({1, 1});
        mesh.add_vertex({0, 1});
        mesh.add_quad(0, 1, 2, 3);

        auto id = compute_facet_area(mesh, options);
        REQUIRE(mesh.template is_attribute_type<Scalar>(id));
        REQUIRE(!mesh.is_attribute_indexed(id));
        REQUIRE(mesh.get_attribute_name(id) == options.output_attribute_name);

        const auto& areas = vector_view(mesh.get_attribute<Scalar>(id));
        REQUIRE_THAT(areas[0], Catch::Matchers::WithinAbs(1.0, eps));
    }

    SECTION("2D polygon")
    {
        SurfaceMesh<Scalar, Index> mesh(2);
        mesh.add_vertex({0, 0});
        mesh.add_vertex({1, 0});
        mesh.add_vertex({2, 0});
        mesh.add_vertex({2, 1});
        mesh.add_vertex({2, 2});
        mesh.add_vertex({1, 2});
        mesh.add_vertex({0, 2});
        mesh.add_vertex({0, 1});
        mesh.add_polygon({0, 1, 2, 3, 4, 5, 6, 7});

        auto id = compute_facet_area(mesh, options);
        REQUIRE(mesh.template is_attribute_type<Scalar>(id));
        REQUIRE(!mesh.is_attribute_indexed(id));
        REQUIRE(mesh.get_attribute_name(id) == options.output_attribute_name);

        const auto& areas = vector_view(mesh.get_attribute<Scalar>(id));
        REQUIRE_THAT(areas[0], Catch::Matchers::WithinAbs(4.0, eps));
    }

    SECTION("3D triangle")
    {
        SurfaceMesh<Scalar, Index> mesh(3);
        mesh.add_vertex({0, 0, 0});
        mesh.add_vertex({1, 0, 0});
        mesh.add_vertex({0, 1, 0});
        mesh.add_vertex({1, 1, 0});
        mesh.add_triangle(0, 1, 2);
        mesh.add_triangle(2, 1, 3);

        auto id = compute_facet_area(mesh, options);
        REQUIRE(mesh.template is_attribute_type<Scalar>(id));
        REQUIRE(!mesh.is_attribute_indexed(id));
        REQUIRE(mesh.get_attribute_name(id) == options.output_attribute_name);

        const auto& areas = vector_view(mesh.get_attribute<Scalar>(id));
        REQUIRE_THAT(areas[0], Catch::Matchers::WithinAbs(0.5, eps));
        REQUIRE_THAT(areas[1], Catch::Matchers::WithinAbs(0.5, eps));
    }

    SECTION("3D quad")
    {
        SurfaceMesh<Scalar, Index> mesh(3);
        mesh.add_vertex({0, 0, 0});
        mesh.add_vertex({1, 0, 0});
        mesh.add_vertex({1, 1, 0});
        mesh.add_vertex({0, 1, 0});
        mesh.add_quad(0, 1, 2, 3);

        auto id = compute_facet_area(mesh, options);
        REQUIRE(mesh.template is_attribute_type<Scalar>(id));
        REQUIRE(!mesh.is_attribute_indexed(id));
        REQUIRE(mesh.get_attribute_name(id) == options.output_attribute_name);

        const auto& areas = vector_view(mesh.get_attribute<Scalar>(id));
        REQUIRE_THAT(areas[0], Catch::Matchers::WithinAbs(1.0, eps));
    }

    SECTION("3D polygon")
    {
        SurfaceMesh<Scalar, Index> mesh(3);
        mesh.add_vertex({0, 0, 0});
        mesh.add_vertex({1, 0, 0});
        mesh.add_vertex({2, 0, 0});
        mesh.add_vertex({2, 1, 0});
        mesh.add_vertex({2, 2, 0});
        mesh.add_vertex({1, 2, 0});
        mesh.add_vertex({0, 2, 0});
        mesh.add_vertex({0, 1, 0});
        mesh.add_polygon({0, 1, 2, 3, 4, 5, 6, 7});

        auto id = compute_facet_area(mesh, options);
        REQUIRE(mesh.template is_attribute_type<Scalar>(id));
        REQUIRE(!mesh.is_attribute_indexed(id));
        REQUIRE(mesh.get_attribute_name(id) == options.output_attribute_name);

        const auto& areas = vector_view(mesh.get_attribute<Scalar>(id));
        REQUIRE_THAT(areas[0], Catch::Matchers::WithinAbs(4.0, eps));
    }

    SECTION("3D quad transformed")
    {
        SurfaceMesh<Scalar, Index> mesh(3);
        mesh.add_vertex({0, 0, 0});
        mesh.add_vertex({1, 0, 0});
        mesh.add_vertex({1, 1, 0});
        mesh.add_vertex({0, 1, 0});
        mesh.add_quad(0, 1, 2, 3);

        auto transformation = Eigen::Transform<Scalar, 3, Eigen::Affine>::Identity();
        transformation.linear().diagonal().fill(2.);

        auto id = compute_facet_area(mesh, transformation, options);
        REQUIRE(mesh.template is_attribute_type<Scalar>(id));
        REQUIRE(!mesh.is_attribute_indexed(id));
        REQUIRE(mesh.get_attribute_name(id) == options.output_attribute_name);

        const auto& areas = vector_view(mesh.get_attribute<Scalar>(id));
        REQUIRE_THAT(areas[0], Catch::Matchers::WithinAbs(4.0, eps));
    }
}

TEST_CASE("compute_facet_area benchmark", "[surface][attribute][area][!benchmark]")
{
    using namespace lagrange;
    using Scalar = double;
    using Index = uint32_t;

    FacetAreaOptions options;
    options.output_attribute_name = "facet_area";

    auto mesh = lagrange::testing::load_surface_mesh<Scalar, Index>("open/core/dragon.obj");
    compute_facet_area(mesh, options);

    BENCHMARK_ADVANCED("compute_area")(Catch::Benchmark::Chronometer meter)
    {
        if (mesh.has_attribute(options.output_attribute_name))
            mesh.delete_attribute(options.output_attribute_name, AttributeDeletePolicy::Force);
        meter.measure([&]() { return compute_facet_area(mesh, options); });
    };

#ifdef LAGRANGE_ENABLE_LEGACY_FUNCTIONS
    using MeshType = TriangleMesh3D;
    auto legacy_mesh = to_legacy_mesh<MeshType>(mesh);

    BENCHMARK_ADVANCED("legacy::compute_facet_area")(Catch::Benchmark::Chronometer meter)
    {
        if (legacy_mesh->has_facet_attribute("area")) legacy_mesh->remove_facet_attribute("area");
        meter.measure([&]() { return compute_facet_area(*legacy_mesh); });
    };
#endif
}


#ifdef LAGRANGE_ENABLE_LEGACY_FUNCTIONS

TEST_CASE("legacy::compute_facet_area 2DTriangleFacetArea", "[mesh][triangle][area]")
{
    using namespace lagrange;

    Vertices2D vertices(4, 2);
    vertices << 0.0, 0.0, 1.0, 0.0, 0.0, 1.0, 1.0, 1.0;
    Triangles facets(2, 3);
    facets << 0, 1, 2, 2, 1, 3;

    auto mesh = create_mesh(vertices, facets);
    compute_facet_area(*mesh);
    REQUIRE(mesh->has_facet_attribute("area"));

    auto& areas = mesh->get_facet_attribute("area");
    REQUIRE(areas.rows() == facets.rows());
    REQUIRE(areas(0, 0) == Catch::Approx(0.5));
    REQUIRE(areas(1, 0) == Catch::Approx(0.5));
}

TEST_CASE("legacy::compute_facet_area 3DTriangleFacetArea", "[mesh][triangle][area]")
{
    using namespace lagrange;

    Vertices3D vertices(4, 3);
    vertices << 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 1.0, 1.0, 0.0;
    Triangles facets(2, 3);
    facets << 0, 1, 2, 2, 1, 3;

    auto mesh = create_mesh(vertices, facets);
    compute_facet_area(*mesh);
    REQUIRE(mesh->has_facet_attribute("area"));

    auto& areas = mesh->get_facet_attribute("area");
    REQUIRE(areas.rows() == facets.rows());
    REQUIRE(areas(0, 0) == Catch::Approx(0.5));
    REQUIRE(areas(1, 0) == Catch::Approx(0.5));
}

TEST_CASE("legacy::compute_facet_area 2DQuadFacetArea", "[mesh][quad][area]")
{
    using namespace lagrange;

    Vertices2D vertices(4, 2);
    vertices << 0.0, 0.0, 1.0, 0.0, 0.0, 1.0, 1.0, 1.0;
    Quads facets(1, 4);
    facets << 0, 1, 3, 2;

    auto mesh = create_mesh(vertices, facets);
    compute_facet_area(*mesh);
    REQUIRE(mesh->has_facet_attribute("area"));

    auto& areas = mesh->get_facet_attribute("area");
    REQUIRE(areas.rows() == facets.rows());
    REQUIRE(areas(0, 0) == Catch::Approx(1.0));
}

TEST_CASE("legacy::compute_facet_area 3DQuadFacetArea", "[mesh][quad][area]")
{
    using namespace lagrange;

    Vertices3D vertices(4, 3);
    vertices << 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 1.0, 1.0, 0.0;
    Quads facets(1, 4);
    facets << 0, 1, 3, 2;

    auto mesh = create_mesh(vertices, facets);
    compute_facet_area(*mesh);
    REQUIRE(mesh->has_facet_attribute("area"));

    auto& areas = mesh->get_facet_attribute("area");
    REQUIRE(areas.rows() == facets.rows());
    REQUIRE(areas(0, 0) == Catch::Approx(1.0));
}

TEST_CASE("legacy::compute_facet_area SingleUVArea", "[mesh][uv][area]")
{
    Eigen::Matrix<float, 3, 2> uv;
    uv << 0.0, 0.0, 1.0, 0.0, 0.0, 1.0;
    Eigen::Matrix<int, 1, 3> triangle;
    triangle << 0, 1, 2;

    const auto areas = lagrange::compute_uv_area_raw(uv, triangle);
    REQUIRE(1 == areas.rows());
    REQUIRE(0.5 == areas[0]);
}

TEST_CASE("legacy::compute_facet_area UVArea", "[mesh][uv][area]")
{
    Eigen::Matrix<double, 5, 2, Eigen::RowMajor> uv;
    uv << 0.0, 0.0, 1.0, 0.0, 0.0, 1.0, 1.0, 1.0, 2.0, 2.0;

    Eigen::Matrix<size_t, 3, 3, Eigen::RowMajor> triangle;
    triangle << 0, 1, 2, // area = 0.5
        1, 2, 3, // area = -0.5
        0, 3, 4; // area = 0.0

    const auto areas = lagrange::compute_uv_area_raw(uv, triangle);
    REQUIRE(3 == areas.rows());
    REQUIRE(0.5 == areas[0]);
    REQUIRE(-0.5 == areas[1]);
    REQUIRE(0.0 == areas[2]);
}
#endif
