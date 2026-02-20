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
#include <lagrange/primitive/generate_rounded_cylinder.h>
#include <lagrange/testing/common.h>
#include <catch2/catch_approx.hpp>
#include "primitive_test_utils.h"

TEST_CASE("RoundedCylinder", "[primitive][rounded_cylinder]" LA_SLOW_DEBUG_FLAG)
{
    using namespace lagrange;

    using MeshType = lagrange::TriangleMesh3D;
    using Scalar = MeshType::Scalar;
    using Index = MeshType::Index;

    auto check_dimension = [](MeshType& mesh, const Scalar radius, const Scalar height) {
        const auto& vertices = mesh.get_vertices();
        if (vertices.rows() == 0) return;
        const auto x_range = vertices.col(0).maxCoeff() - vertices.col(0).minCoeff();
        const auto y_range = vertices.col(1).maxCoeff() - vertices.col(1).minCoeff();
        const auto z_range = vertices.col(2).maxCoeff() - vertices.col(2).minCoeff();
        REQUIRE(x_range <= Catch::Approx(2 * radius));
        REQUIRE(y_range == Catch::Approx(height));
        REQUIRE(z_range <= Catch::Approx(2 * radius));
    };

    SECTION("Simple cylinder")
    {
        Scalar r = 2.0, h = 5.0, bevel = 0;
        Index segments = 1;
        Index n;
        SECTION("Min sections")
        {
            n = 3;
        }
        SECTION("Many sections")
        {
            n = (Index)1e2;
        }
        auto mesh =
            lagrange::primitive::generate_rounded_cylinder<MeshType>(r, h, bevel, n, segments);
        primitive_test_utils::validate_primitive(*mesh);
        primitive_test_utils::check_degeneracy(*mesh);
        check_dimension(*mesh, r, h);
        primitive_test_utils::check_semantic_labels(*mesh);
        primitive_test_utils::check_UV(*mesh);
    }

    SECTION("Rounded cylinder")
    {
        Scalar r = 2.0, h = 5.0, bevel = 1;
        Index sections = 50;
        Index n;
        SECTION("Single segment")
        {
            n = 1;
        }
        SECTION("Many segments")
        {
            n = (Index)1e2;
        }
        auto mesh =
            lagrange::primitive::generate_rounded_cylinder<MeshType>(r, h, bevel, sections, n);
        primitive_test_utils::validate_primitive(*mesh);
        primitive_test_utils::check_degeneracy(*mesh);
        check_dimension(*mesh, r, h);
        primitive_test_utils::check_semantic_labels(*mesh);
        primitive_test_utils::check_UV(*mesh);
    }

    SECTION("Rounded cylinder, Comparison to 2*lagrange::internal::pi and slice")
    {
        Scalar r = 2.0, h = 5.0, bevel = 1;
        Index sections = 50;
        Index n = 20;
        Scalar begin_angle = 0.0;
        Scalar sweep_angle;
        SECTION("sweep1")
        {
            sweep_angle = 2 * lagrange::internal::pi + 2e-8;
        }
        SECTION("sweep2")
        {
            sweep_angle = 3 / 4. * lagrange::internal::pi;
        }
        auto mesh = lagrange::primitive::generate_rounded_cylinder<MeshType>(
            r,
            h,
            bevel,
            sections,
            n,
            begin_angle,
            sweep_angle);
        primitive_test_utils::validate_primitive(*mesh);
        primitive_test_utils::check_degeneracy(*mesh);
        check_dimension(*mesh, r, h);
        primitive_test_utils::check_semantic_labels(*mesh);
        primitive_test_utils::check_UV(*mesh);
    }

    SECTION("Simple cylinder: Zero geometry")
    {
        Scalar r = 2.0, h = 5.0, bevel = 0;
        Index segments = 1;
        Index sections = 50;
        SECTION("r=0")
        {
            r = 0.0;
        }

        SECTION("h=0")
        {
            h = 0.0;
        }

        SECTION("r=0, h=0")
        {
            r = 0.0;
            h = 0.0;
        }
        auto mesh = lagrange::primitive::generate_rounded_cylinder<MeshType>(
            r,
            h,
            bevel,
            sections,
            segments);
        REQUIRE(mesh->get_vertices().hasNaN() == false);
    }

    SECTION("Invalid dimension")
    {
        Scalar r = -2.0, h = 5.0, bevel = -1.0;
        Index sections = 50, segments = 0;
        auto mesh = lagrange::primitive::generate_rounded_cylinder<MeshType>(
            r,
            h,
            bevel,
            sections,
            segments);
        REQUIRE(mesh->get_vertices().hasNaN() == false);
        check_dimension(*mesh, 0, h);
        primitive_test_utils::check_semantic_labels(*mesh);
    }

    SECTION("Config struct")
    {
        lagrange::primitive::RoundedCylinderConfig config;
        config.height = 2;
        config.bevel_radius = 1;
        config.center = {0, 1, 0};
        auto mesh = lagrange::primitive::generate_rounded_cylinder<MeshType>(config);
        primitive_test_utils::validate_primitive(*mesh);
        primitive_test_utils::check_degeneracy(*mesh);
        primitive_test_utils::check_semantic_labels(*mesh);
        primitive_test_utils::check_UV(*mesh);

        const auto& vertices = mesh->get_vertices();
        REQUIRE(vertices.col(1).minCoeff() > -std::numeric_limits<Scalar>::epsilon());
    }
}
