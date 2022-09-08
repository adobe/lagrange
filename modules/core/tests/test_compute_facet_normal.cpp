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
#include <lagrange/compute_triangle_normal.h>
#include <lagrange/mesh_convert.h>
#include <lagrange/testing/common.h>
#include <lagrange/utils/geometry3d.h>
#include <lagrange/views.h>
#include <catch2/catch_approx.hpp>

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
        REQUIRE((normals - ground_truth).squaredNorm() == Catch::Approx(0));
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

        FacetNormalOptions options;
        options.output_attribute_name = "normal";
        auto id = compute_facet_normal(mesh, options);
        REQUIRE(mesh.has_attribute("normal"));
        REQUIRE(mesh.get_attribute_id("normal") == id);
        REQUIRE(mesh.template is_attribute_type<Scalar>(id));
        REQUIRE(!mesh.is_attribute_indexed(id));

        Eigen::Matrix<Scalar, 6, 3, Eigen::RowMajor> ground_truth;
        ground_truth << 0, 0, -1, 0, 0, 1, 0, -1, 0, 0, 1, 0, 1, 0, 0, -1, 0, 0;
        const auto& normals = matrix_view(mesh.get_attribute<Scalar>(id));
        REQUIRE((normals - ground_truth).squaredNorm() == Catch::Approx(0));
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
        REQUIRE((normals - ground_truth.transpose()).squaredNorm() == Catch::Approx(0));
    }
}

#ifdef LAGRANGE_ENABLE_LEGACY_FUNCTIONS

TEST_CASE(
    "legacy::compute_triangle_normal vs compute_facet_normal",
    "[mesh][attribute][normal][legacy]")
{
    using namespace lagrange;
    using Scalar = double;
    using Index = uint32_t;
    using MeshType = TriangleMesh3D;

    auto mesh = lagrange::testing::load_surface_mesh<Scalar, Index>("open/core/blub/blub.obj");
    auto id = compute_facet_normal(mesh);
    auto new_normals = matrix_view(mesh.template get_attribute<Scalar>(id));

    using MeshType = TriangleMesh3D;
    auto legacy_mesh = to_legacy_mesh<MeshType>(mesh);
    CHECK(!legacy_mesh->has_facet_attribute("normal"));
    compute_triangle_normal(*legacy_mesh);
    CHECK(legacy_mesh->has_facet_attribute("normal"));

    auto new_mesh = to_surface_mesh_copy<Scalar, Index>(*legacy_mesh);
    auto old_normals = matrix_view(new_mesh.template get_attribute<Scalar>("normal"));

    const auto num_facets = mesh.get_num_facets();
    for (auto i : range(num_facets)) {
        const Eigen::Matrix<Scalar, 1, 3> new_normal = new_normals.row(i);
        const Eigen::Matrix<Scalar, 1, 3> old_normal = old_normals.row(i);
        INFO("new_normal=(" << new_normal << ")");
        INFO("old_normal=(" << old_normal << ")");
        REQUIRE(angle_between(new_normal, old_normal) == Catch::Approx(0).margin(1e-3));
    }
}

#endif
