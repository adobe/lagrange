/*
 * Copyright 2019 Adobe. All rights reserved.
 * This file is licensed to you under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License. You may obtain a copy
 * of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under
 * the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
 * OF ANY KIND, either express or implied. See the License for the specific language
 * governing permissions and limitations under the License.
 */
#include <lagrange/common.h>
#include <lagrange/compute_euler.h>
#include <lagrange/io/save_mesh.h>
#include <lagrange/primitive/generate_sphere.h>
#include <lagrange/testing/common.h>
#include <lagrange/uv_mesh.h>
#include <lagrange/views.h>

#include <catch2/catch_approx.hpp>
#include "primitive_test_utils.h"

TEST_CASE("generate_sphere", "[primitive][surface]")
{
    using namespace lagrange;
    using Scalar = float;
    using Index = uint32_t;

    primitive::SphereOptions setting;
    setting.triangulate = true;

    SECTION("default setting")
    {
        auto mesh = lagrange::primitive::generate_sphere<Scalar, Index>(setting);
        primitive_test_utils::validate_primitive(mesh);
        primitive_test_utils::check_degeneracy(mesh);
        primitive_test_utils::check_UV(mesh);
    }

    SECTION("fixed uv")
    {
        setting.fixed_uv = true;
        auto mesh = lagrange::primitive::generate_sphere<Scalar, Index>(setting);
        primitive_test_utils::validate_primitive(mesh);
        primitive_test_utils::check_degeneracy(mesh);
        primitive_test_utils::check_UV(mesh);
    }

    SECTION("with cross section")
    {
        setting.start_sweep_angle = 0.0;
        setting.end_sweep_angle = static_cast<Scalar>(3);
        auto mesh = lagrange::primitive::generate_sphere<Scalar, Index>(setting);
        primitive_test_utils::validate_primitive(mesh);
        primitive_test_utils::check_degeneracy(mesh);
        primitive_test_utils::check_UV(mesh);
    }

    SECTION("radius 0")
    {
        setting.radius = 0.0;
        auto mesh = lagrange::primitive::generate_sphere<Scalar, Index>(setting);
        REQUIRE(mesh.get_num_facets() == 0);
    }

    SECTION("fixed vs non-fixed UV")
    {
        setting.fixed_uv = false;
        auto mesh1 = lagrange::primitive::generate_sphere<Scalar, Index>(setting);
        setting.fixed_uv = true;
        auto mesh2 = lagrange::primitive::generate_sphere<Scalar, Index>(setting);

        primitive_test_utils::validate_primitive(mesh1);
        primitive_test_utils::validate_primitive(mesh2);
        primitive_test_utils::check_degeneracy(mesh1);
        primitive_test_utils::check_degeneracy(mesh2);
        primitive_test_utils::check_UV(mesh1);
        primitive_test_utils::check_UV(mesh2);

        auto uv_mesh1 = uv_mesh_view(mesh1);
        auto uv_mesh2 = uv_mesh_view(mesh2);
        auto uv1 = vertex_view(uv_mesh1);
        auto uv2 = vertex_view(uv_mesh2);
        REQUIRE((uv1 - uv2).norm() < 1e-6f);
    }
}

#ifdef LAGRANGE_ENABLE_LEGACY_FUNCTIONS
TEST_CASE("Sphere", "[primitive][sphere]" LA_SLOW_DEBUG_FLAG)
{
    using namespace lagrange;
    using namespace lagrange::primitive_test_utils;

    using MeshType = lagrange::TriangleMesh3D;
    using Scalar = MeshType::Scalar;
    using Index = MeshType::Index;
    Scalar radius = 2.0;

    auto check_dimension = [&radius](MeshType& mesh) {
        const auto& vertices = mesh.get_vertices();
        const auto x_range = vertices.col(0).maxCoeff() - vertices.col(0).minCoeff();
        const auto y_range = vertices.col(1).maxCoeff() - vertices.col(1).minCoeff();
        const auto z_range = vertices.col(2).maxCoeff() - vertices.col(2).minCoeff();
        REQUIRE(x_range <= Catch::Approx(2 * radius));
        REQUIRE(y_range <= Catch::Approx(2 * radius));
        REQUIRE(z_range <= Catch::Approx(2 * radius));
    };

    auto check_normal = [](MeshType& mesh) {
        REQUIRE(mesh.has_indexed_attribute("normal"));
        REQUIRE(mesh.has_facet_attribute("normal"));

        const auto& r = mesh.get_indexed_attribute("normal");
        const auto& normals = std::get<0>(r);
        const auto& indices = std::get<1>(r);
        const auto& triangle_normals = mesh.get_facet_attribute("normal");
        for (auto i : range(mesh.get_num_facets())) {
            for (auto j : range(mesh.get_vertex_per_facet())) {
                REQUIRE(
                    (normals.row(indices(i, j)) - triangle_normals.row(i)).norm() < sqrt(3) / 2);
            }
        }
    };

    auto check_poles = [](MeshType& mesh) {
        const auto& uvs = mesh.get_uv();
        const auto& uv_indices = mesh.get_uv_indices();
        const auto& facets = mesh.get_facets();
        const auto num_facets = mesh.get_num_facets();
        const auto vertex_per_facet = mesh.get_vertex_per_facet();

        const auto min_v = uvs.col(1).minCoeff();
        const auto max_v = uvs.col(1).maxCoeff();

        for (auto i : range(num_facets)) {
            for (auto j : range(vertex_per_facet)) {
                if (facets(i, j) == 0) {
                    // North pole!
                    REQUIRE(uvs(uv_indices(i, j), 1) == Catch::Approx(max_v).margin(1e-3));
                } else if (facets(i, j) == 1) {
                    // South pole!
                    REQUIRE(uvs(uv_indices(i, j), 1) == Catch::Approx(min_v).margin(1e-3));
                }
            }
        }
    };

    SECTION("Sphere: Sections")
    {
        Index n;
        Eigen::Matrix<Scalar, 3, 1> center = Eigen::Matrix<Scalar, 3, 1>(0.0, 0.0, 0.0);
        SECTION("Min Sections")
        {
            n = 3;
        }
        SECTION("Many Sections")
        {
            n = (Index)1e2;
        }
        auto mesh = lagrange::primitive::generate_sphere<MeshType>(
            radius,
            center,
            2 * lagrange::internal::pi,
            n);
        primitive_test_utils::validate_primitive(*mesh);
        primitive_test_utils::check_degeneracy(*mesh);
        check_dimension(*mesh);
        primitive_test_utils::check_semantic_labels(*mesh);
        primitive_test_utils::check_UV(*mesh);
        check_normal(*mesh);
        check_poles(*mesh);
    }

    SECTION("Invalid dimension")
    {
        Scalar r = -2.0;
        Index sections = 0;
        Eigen::Matrix<Scalar, 3, 1> center = Eigen::Matrix<Scalar, 3, 1>(0.0, 0.0, 0.0);
        auto mesh = lagrange::primitive::generate_sphere<MeshType>(
            r,
            center,
            2 * lagrange::internal::pi,
            sections);
        primitive_test_utils::validate_primitive(*mesh);
    }

    SECTION("Debug sphere test")
    {
        auto mesh = lagrange::primitive::generate_sphere<MeshType>(0.5);
        validate_primitive(*mesh, false);
        check_degeneracy(*mesh);
        check_semantic_labels(*mesh);
        auto euler = compute_euler(*mesh);
        REQUIRE(euler == 2);

        auto uv_mesh = mesh->get_uv_mesh();
        REQUIRE(compute_euler(*uv_mesh) == 1);
        primitive_test_utils::check_UV(*mesh);
        check_normal(*mesh);
        check_poles(*mesh);
    }

    SECTION("Off center sphere")
    {
        auto mesh = lagrange::primitive::generate_sphere<MeshType>(0.5, {1, 1, 1});
        validate_primitive(*mesh, false);
        check_degeneracy(*mesh);
        check_semantic_labels(*mesh);
        auto euler = compute_euler(*mesh);
        REQUIRE(euler == 2);
        primitive_test_utils::check_UV(*mesh);
        check_normal(*mesh);
        check_poles(*mesh);
        REQUIRE(mesh->has_indexed_attribute("normal"));
    }

    SECTION("Config struct")
    {
        LA_IGNORE_DEPRECATION_WARNING_BEGIN
        lagrange::primitive::SphereConfig config;
        config.output_normals = false;
        config.num_longitude_sections = 32;
        config.num_latitude_sections = 16;
        config.sweep_angle = 1;
        config.center = {0, 0, 1};
        auto mesh = lagrange::primitive::generate_sphere<MeshType>(config);
        LA_IGNORE_DEPRECATION_WARNING_END
        validate_primitive(*mesh, 0);
        check_degeneracy(*mesh);
        check_semantic_labels(*mesh);

        REQUIRE(!mesh->has_indexed_attribute("normal"));
    }
}
#endif
