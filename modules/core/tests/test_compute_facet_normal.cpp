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
#include <lagrange/Attribute.h>
#include <lagrange/compute_facet_normal.h>
#include <lagrange/testing/common.h>
#include <lagrange/views.h>

TEST_CASE("compute_facet_normal", "[core][normal]")
{
    using namespace lagrange;
    using Scalar = double;
    using Index = uint32_t;

    SECTION("tet")
    {
        SurfaceMesh<Scalar, Index> mesh;
        mesh.add_vertex({0, 0, 0});
        mesh.add_vertex({1, 0, 0});
        mesh.add_vertex({0, 1, 0});
        mesh.add_vertex({0, 0, 1});

        mesh.add_triangle(0, 2, 1);
        mesh.add_triangle(0, 3, 2);
        mesh.add_triangle(0, 1, 3);
        mesh.add_triangle(1, 2, 3);

        auto id = compute_facet_normal(mesh);
        REQUIRE(mesh.template is_attribute_type<Scalar>(id));
        REQUIRE(!mesh.is_attribute_indexed(id));

        const auto& normals = matrix_view(mesh.get_attribute<Scalar>(id));
        Eigen::Matrix<Scalar, 4, 3, Eigen::RowMajor> ground_truth;
        ground_truth << 0, 0, -1, -1, 0, 0, 0, -1, 0, 1, 1, 1;
        ground_truth.rowwise().normalize();
        REQUIRE((normals - ground_truth).squaredNorm() == Approx(0));
    }

    SECTION("Cube")
    {
        SurfaceMesh<Scalar, Index> mesh;
        mesh.add_vertex({0, 0, 0});
        mesh.add_vertex({1, 0, 0});
        mesh.add_vertex({1, 1, 0});
        mesh.add_vertex({0, 1, 0});
        mesh.add_vertex({0, 0, 1});
        mesh.add_vertex({1, 0, 1});
        mesh.add_vertex({1, 1, 1});
        mesh.add_vertex({0, 1, 1});
        mesh.add_quad(0, 3, 2, 1);
        mesh.add_quad(4, 5, 6, 7);
        mesh.add_quad(0, 1, 5, 4);
        mesh.add_quad(2, 3, 7, 6);
        mesh.add_quad(1, 2, 6, 5);
        mesh.add_quad(3, 0, 4, 7);

        auto id = compute_facet_normal(mesh, "normal");
        REQUIRE(mesh.has_attribute("normal"));
        REQUIRE(mesh.get_attribute_id("normal") == id);
        REQUIRE(mesh.template is_attribute_type<Scalar>(id));
        REQUIRE(!mesh.is_attribute_indexed(id));

        Eigen::Matrix<Scalar, 6, 3, Eigen::RowMajor> ground_truth;
        ground_truth << 0, 0, -1, 0, 0, 1, 0, -1, 0, 0, 1, 0, 1, 0, 0, -1, 0, 0;
        const auto& normals = matrix_view(mesh.get_attribute<Scalar>(id));
        REQUIRE((normals - ground_truth).squaredNorm() == Approx(0));
    }

    SECTION("Non-planar quad")
    {
        SurfaceMesh<Scalar, Index> mesh;
        mesh.add_vertex({0, 0, 0});
        mesh.add_vertex({1, 0, 1});
        mesh.add_vertex({1, 1, 0});
        mesh.add_vertex({0, 1, 1});
        mesh.add_quad(0, 1, 2, 3);

        auto id = compute_facet_normal(mesh);
        const auto& normals = matrix_view(mesh.get_attribute<Scalar>(id));
        Eigen::Vector3<Scalar> ground_truth(0, 0, 1);
        REQUIRE((normals - ground_truth.transpose()).squaredNorm() == Approx(0));
    }
}
