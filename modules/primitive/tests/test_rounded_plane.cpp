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

#include <lagrange/IndexedAttribute.h>
#include <lagrange/common.h>
#include <lagrange/primitive/generate_rounded_plane.h>
#include <lagrange/testing/common.h>
#include <lagrange/views.h>

#include "primitive_test_utils.h"

#include <catch2/matchers/catch_matchers_floating_point.hpp>

TEST_CASE("generate_rounded_plane", "[primitive][surface]")
{
    using namespace lagrange;
    using Scalar = float;
    using Index = uint32_t;

    primitive::RoundedPlaneOptions setting;

    SECTION("Simple rectangle")
    {
        auto mesh = primitive::generate_rounded_plane<Scalar, Index>(setting);
        primitive_test_utils::validate_primitive(mesh, 1);
        primitive_test_utils::check_degeneracy(mesh);
        primitive_test_utils::check_UV(mesh);
    }

    SECTION("Rounded rectangle")
    {
        setting.width = 2.0f;
        setting.height = 1.0f;
        setting.width_segments = 5;
        setting.height_segments = 2;
        setting.bevel_radius = 0.2f;
        setting.bevel_segments = 5;

        auto mesh = primitive::generate_rounded_plane<Scalar, Index>(setting);
        primitive_test_utils::validate_primitive(mesh, 1);
        primitive_test_utils::check_degeneracy(mesh);
        primitive_test_utils::check_UV(mesh);
    }

    SECTION("Circle")
    {
        setting.width = 1.0f;
        setting.height = 1.0f;
        setting.width_segments = 5;
        setting.height_segments = 2;
        setting.bevel_radius = 0.5f;
        setting.bevel_segments = 16;

        auto mesh = primitive::generate_rounded_plane<Scalar, Index>(setting);
        primitive_test_utils::validate_primitive(mesh, 1);
        primitive_test_utils::check_degeneracy(mesh);
        primitive_test_utils::check_UV(mesh);
    }

    SECTION("Capsule")
    {
        setting.width = 1.0f;
        setting.height = 2.0f;
        setting.width_segments = 5;
        setting.height_segments = 2;
        setting.bevel_radius = 0.5f;
        setting.bevel_segments = 16;

        auto mesh = primitive::generate_rounded_plane<Scalar, Index>(setting);
        primitive_test_utils::validate_primitive(mesh, 1);
        primitive_test_utils::check_degeneracy(mesh);
        primitive_test_utils::check_UV(mesh);
    }

    SECTION("Zero height")
    {
        setting.height = 0.0f;
        auto mesh = primitive::generate_rounded_plane<Scalar, Index>(setting);
        REQUIRE(mesh.get_num_vertices() == 0);
    }

    SECTION("XZ plane")
    {
        setting.normal = {0, 1, 0};
        auto mesh = primitive::generate_rounded_plane<Scalar, Index>(setting);
        primitive_test_utils::validate_primitive(mesh, 1);
        primitive_test_utils::check_degeneracy(mesh);
        primitive_test_utils::check_UV(mesh);

        // Check normal direction
        REQUIRE(mesh.has_attribute(setting.normal_attribute_name));
        REQUIRE(mesh.is_attribute_indexed(setting.normal_attribute_name));
        auto& normal_attr = mesh.get_indexed_attribute<Scalar>(setting.normal_attribute_name);
        auto& normal_values = normal_attr.values();
        auto normals = matrix_view(normal_values);
        for (Index i = 0; i < normals.rows(); i++) {
            REQUIRE_THAT(normals(i, 0), Catch::Matchers::WithinAbs(0.0f, 1e-6f));
            REQUIRE_THAT(normals(i, 1), Catch::Matchers::WithinAbs(1.0f, 1e-6f));
            REQUIRE_THAT(normals(i, 2), Catch::Matchers::WithinAbs(0.0f, 1e-6f));
        }
    }
}

#ifdef LAGRANGE_ENABLE_LEGACY_FUNCTIONS

TEST_CASE("RoundedPlane", "[primitive][rounded_plane]")
{
    using namespace lagrange;

    using MeshType = lagrange::TriangleMesh3D;
    using Scalar = MeshType::Scalar;
    using Index = MeshType::Index;

    auto check_dimension = [](MeshType& mesh, const Scalar width, const Scalar height) {
        const auto& vertices = mesh.get_vertices();
        const auto x_range = vertices.col(0).maxCoeff() - vertices.col(0).minCoeff();
        const auto y_range = vertices.col(1).maxCoeff() - vertices.col(1).minCoeff();
        const auto z_range = vertices.col(2).maxCoeff() - vertices.col(2).minCoeff();
        REQUIRE(x_range == Catch::Approx(width));
        REQUIRE(y_range == Catch::Approx(0.0));
        REQUIRE(z_range == Catch::Approx(height));
    };

    SECTION("Simple square")
    {
        Scalar w = 1.0, h = 1.0, r = 0.0;
        Index n = 1;
        SECTION("Single segment")
        {
            n = 1;
        }
        SECTION("Many segments")
        {
            n = (Index)1e2;
        }
        auto mesh = lagrange::primitive::generate_rounded_plane<MeshType>(w, h, r, n);
        primitive_test_utils::validate_primitive(*mesh, 1);
        primitive_test_utils::check_degeneracy(*mesh);
        check_dimension(*mesh, w, h);
        primitive_test_utils::check_semantic_labels(*mesh);
        primitive_test_utils::check_UV(*mesh);
    }

    SECTION("Rounded square")
    {
        Scalar w = 1.0, h = 1.0, r = 0.25;
        Index n = 1;
        SECTION("Single segment")
        {
            n = 1;
        }
        SECTION("Many segments")
        {
            n = (Index)1e2;
        }
        auto mesh = lagrange::primitive::generate_rounded_plane<MeshType>(w, h, r, n);
        primitive_test_utils::validate_primitive(*mesh, 1);
        primitive_test_utils::check_degeneracy(*mesh);
        check_dimension(*mesh, w, h);
        primitive_test_utils::check_semantic_labels(*mesh);
        primitive_test_utils::check_UV(*mesh);
    }

    SECTION("Simple circle")
    {
        Scalar w = 1.0, h = 1.0, r = 0.5;
        Index n = 1;
        SECTION("Single segment")
        {
            n = 1;
        }
        SECTION("Many segments")
        {
            n = (Index)1e2;
        }
        auto mesh = lagrange::primitive::generate_rounded_plane<MeshType>(w, h, r, n);
        primitive_test_utils::validate_primitive(*mesh, 1);
        primitive_test_utils::check_degeneracy(*mesh);
        check_dimension(*mesh, w, h);
        primitive_test_utils::check_semantic_labels(*mesh);
        primitive_test_utils::check_UV(*mesh);
    }

    SECTION("Rounded rectangle")
    {
        Scalar w = 2.0, h = 1.0, r = 0.25;
        Index n = 1;
        SECTION("Single segment")
        {
            n = 1;
        }
        SECTION("Many segments")
        {
            n = (Index)1e2;
        }
        auto mesh = lagrange::primitive::generate_rounded_plane<MeshType>(w, h, r, n);
        primitive_test_utils::validate_primitive(*mesh, 1);
        primitive_test_utils::check_degeneracy(*mesh);
        check_dimension(*mesh, w, h);
        primitive_test_utils::check_semantic_labels(*mesh);
        primitive_test_utils::check_UV(*mesh);
    }

    SECTION("Simple Plane: Zero geometry")
    {
        Scalar w = 1.0, h = 1.0, r = 0.0;
        Index n = 1;
        SECTION("w=0")
        {
            w = 0.0;
        }

        SECTION("h=0")
        {
            h = 0.0;
        }

        SECTION("w=0, h=0")
        {
            w = 0.0;
            h = 0.0;
        }

        auto mesh = lagrange::primitive::generate_rounded_plane<MeshType>(w, h, r, n);
        REQUIRE(mesh->get_vertices().hasNaN() == false);
    }

    SECTION("Invalid dimension")
    {
        Scalar w = -0.1, h = 1.0, r = 0.25;
        Index n = 1;
        auto mesh = lagrange::primitive::generate_rounded_plane<MeshType>(w, h, r, n);
        REQUIRE(mesh->get_vertices().hasNaN() == false);
        REQUIRE(mesh->get_num_vertices() == 0);
        REQUIRE(mesh->get_num_facets() == 0);
        primitive_test_utils::check_semantic_labels(*mesh);
    }

    SECTION("Config struct")
    {
        lagrange::primitive::RoundedPlaneConfig config;
        config.radius = 0.1;
        config.center = {1, 1, 1};
        config.output_normals = false;
        auto mesh = lagrange::primitive::generate_rounded_plane<MeshType>(config);
        check_dimension(*mesh, config.width, config.height);
        primitive_test_utils::check_semantic_labels(*mesh);
        primitive_test_utils::validate_primitive(*mesh, 1);
        primitive_test_utils::check_degeneracy(*mesh);
        primitive_test_utils::check_UV(*mesh);
        REQUIRE(!mesh->has_indexed_attribute("normal"));
    }
}

#endif
