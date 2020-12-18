/*
 * Copyright 2020 Adobe. All rights reserved.
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

#include <lagrange/Mesh.h>
#include <lagrange/common.h>
#include <lagrange/compute_facet_area.h>
#include <lagrange/create_mesh.h>

TEST_CASE("2DTriangleFacetArea", "[mesh][triangle][area]")
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
    REQUIRE(areas(0, 0) == Approx(0.5));
    REQUIRE(areas(1, 0) == Approx(0.5));
}

TEST_CASE("3DTriangleFacetArea", "[mesh][triangle][area]")
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
    REQUIRE(areas(0, 0) == Approx(0.5));
    REQUIRE(areas(1, 0) == Approx(0.5));
}

TEST_CASE("2DQuadFacetArea", "[mesh][quad][area]")
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
    REQUIRE(areas(0, 0) == Approx(1.0));
}

TEST_CASE("3DQuadFacetArea", "[mesh][quad][area]")
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
    REQUIRE(areas(0, 0) == Approx(1.0));
}

TEST_CASE("SingleUVArea", "[mesh][uv][area]")
{
    Eigen::Matrix<float, 3, 2> uv;
    uv << 0.0, 0.0, 1.0, 0.0, 0.0, 1.0;
    Eigen::Matrix<int, 1, 3> triangle;
    triangle << 0, 1, 2;

    const auto areas = lagrange::compute_uv_area_raw(uv, triangle);
    REQUIRE(1 == areas.rows());
    REQUIRE(0.5 == areas[0]);
}

TEST_CASE("UVArea", "[mesh][uv][area]")
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
