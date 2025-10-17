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

#include <lagrange/compute_area.h>
#include <lagrange/testing/common.h>
#include <lagrange/views.h>

#include <Eigen/Dense>

#include <catch2/matchers/catch_matchers_floating_point.hpp>

TEST_CASE("compute_facet_vector_area", "[core][surface][area]")
{
    using namespace lagrange;
    using Scalar = double;
    using Index = uint32_t;

    SECTION("Triangle")
    {
        SurfaceMesh<Scalar, Index> mesh;
        mesh.add_vertex({1, 0, 0});
        mesh.add_vertex({0, 1, 0});
        mesh.add_vertex({0, 0, 1});
        mesh.add_triangle(0, 1, 2);

        auto id = compute_facet_vector_area(mesh);
        REQUIRE(mesh.template is_attribute_type<Scalar>(id));
        auto vec_area = attribute_matrix_view<Scalar>(mesh, id);
        auto area = vec_area.row(0).norm();
        REQUIRE_THAT(area, Catch::Matchers::WithinAbs(Scalar(std::sqrt(3) / 2), 1e-9f));

        Eigen::Vector3<Scalar> expected_normal = {1, 1, 1};
        expected_normal.normalize();
        REQUIRE_THAT(
            expected_normal.cross(vec_area.row(0).template head<3>()).norm(),
            Catch::Matchers::WithinAbs(0, 1e-9f));
    }

    SECTION("Non-planar quad")
    {
        SurfaceMesh<Scalar, Index> mesh;
        mesh.add_vertex({10, 0, -0.5f});
        mesh.add_vertex({11, 0, 0});
        mesh.add_vertex({11, 1, -0.5f});
        mesh.add_vertex({10, 1, 0});
        mesh.add_quad(0, 1, 2, 3);

        auto id = compute_facet_vector_area(mesh);
        REQUIRE(mesh.template is_attribute_type<Scalar>(id));
        auto vec_area = attribute_matrix_view<Scalar>(mesh, id);
        auto area = vec_area.row(0).norm();
        REQUIRE_THAT(area, Catch::Matchers::WithinAbs(1, 1e-9f));

        Eigen::Vector3<Scalar> expected_normal = {0, 0, 1};
        expected_normal.normalize();
        REQUIRE_THAT(
            expected_normal.cross(vec_area.row(0).template head<3>()).norm(),
            Catch::Matchers::WithinAbs(0, 1e-9f));
    }
}
