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
#include <lagrange/mesh_cleanup/detect_degenerate_triangles.h>
#include <lagrange/primitive/generate_rounded_cube.h>
#include <lagrange/testing/common.h>
#include <catch2/catch_approx.hpp>

#include "primitive_test_utils.h"

TEST_CASE("generate_rounded_cube", "[primitive][surface]")
{
    using namespace lagrange;
    using Scalar = float;
    using Index = uint32_t;

    primitive::RoundedCubeOptions settings;

    auto check_mesh = [&](auto& mesh) {
        if (mesh.get_num_facets() > 0) {
            REQUIRE(mesh.has_attribute(settings.semantic_label_attribute_name));
            REQUIRE(mesh.has_attribute(settings.normal_attribute_name));
            REQUIRE(mesh.has_attribute(settings.uv_attribute_name));

            primitive_test_utils::validate_primitive(mesh, 0);
            primitive_test_utils::check_degeneracy(mesh);
            primitive_test_utils::check_UV(mesh);
        } else {
            REQUIRE(mesh.get_num_vertices() == 0);
        }
    };

    for (size_t i = 0; i <= 4; i++) {
        settings.width = 0.25f * i;
        for (size_t j = 0; j <= 4; j++) {
            settings.height = 0.25f * j;
            for (size_t k = 0; k <= 4; k++) {
                settings.depth = 0.25f * k;
                for (size_t l = 0; l <= 4; l++) {
                    settings.bevel_radius = 0.25f * l;

                    settings.fixed_uv = false;
                    auto mesh = primitive::generate_rounded_cube<Scalar, Index>(settings);
                    check_mesh(mesh);

                    settings.fixed_uv = true;
                    mesh = primitive::generate_rounded_cube<Scalar, Index>(settings);
                    check_mesh(mesh);
                }
            }
        }
    }
}

#ifdef LAGRANGE_ENABLE_LEGACY_FUNCTIONS
TEST_CASE("RoundedCube", "[primitive][rounded_cube]" LA_SLOW_DEBUG_FLAG)
{
    using namespace lagrange;

    using Scalar = float;
    using Index = uint32_t;
    using MeshType =
        Mesh<Eigen::Matrix<Scalar, Eigen::Dynamic, 3>, Eigen::Matrix<Index, Eigen::Dynamic, 3>>;

    auto check_dimension =
        [](MeshType& mesh, const Scalar width, const Scalar height, const Scalar depth) {
            const auto& vertices = mesh.get_vertices();
            const auto x_range = vertices.col(0).maxCoeff() - vertices.col(0).minCoeff();
            const auto y_range = vertices.col(1).maxCoeff() - vertices.col(1).minCoeff();
            const auto z_range = vertices.col(2).maxCoeff() - vertices.col(2).minCoeff();
            REQUIRE(x_range == Catch::Approx(width));
            REQUIRE(y_range == Catch::Approx(height));
            REQUIRE(z_range == Catch::Approx(depth));
        };

    SECTION("Simple cube")
    {
        Scalar w = 1.0, h = 1.0, d = 1.0, r = 0.0;
        Index n = 1;
        SECTION("Single segment")
        {
            n = 1;
        }
        SECTION("Many segments")
        {
            n = (Index)1e2;
        }
        auto mesh = lagrange::primitive::generate_rounded_cube<MeshType>(w, h, d, r, n);
        primitive_test_utils::validate_primitive(*mesh);
        primitive_test_utils::check_degeneracy(*mesh);
        check_dimension(*mesh, w, h, d);
        primitive_test_utils::check_semantic_labels(*mesh);
    }

    SECTION("Rounded cube")
    {
        Scalar w = 1.0, h = 1.0, d = 1.0, r = 0.25;
        Index n = 1;
        SECTION("Single segment")
        {
            n = 1;
        }
        SECTION("Many segments")
        {
            n = (Index)1e2;
        }
        auto mesh = lagrange::primitive::generate_rounded_cube<MeshType>(w, h, d, r, n);
        primitive_test_utils::validate_primitive(*mesh);
        primitive_test_utils::check_degeneracy(*mesh);
        check_dimension(*mesh, w, h, d);
        primitive_test_utils::check_semantic_labels(*mesh);
    }

    SECTION("Simple sphere")
    {
        Scalar w = 1.0, h = 1.0, d = 1.0, r = 0.5;
        Index n = 1;
        SECTION("Single segment")
        {
            n = 1;
        }
        SECTION("Many segments")
        {
            n = (Index)1e2;
        }
        auto mesh = lagrange::primitive::generate_rounded_cube<MeshType>(w, h, d, r, n);
        primitive_test_utils::validate_primitive(*mesh);
        primitive_test_utils::check_degeneracy(*mesh);
        check_dimension(*mesh, w, h, d);
        primitive_test_utils::check_semantic_labels(*mesh);
    }

    SECTION("Zero radius")
    {
        Scalar w = 20.0, h = 20.0, d = 20.0, r = 0.0;
        Index n = 10;
        auto mesh = lagrange::primitive::generate_rounded_cube<MeshType>(w, h, d, r, n);
        REQUIRE(mesh->get_num_vertices() == 8);
        REQUIRE(mesh->get_num_facets() == 12);
        primitive_test_utils::validate_primitive(*mesh);
        primitive_test_utils::check_degeneracy(*mesh);
        check_dimension(*mesh, w, h, d);
        primitive_test_utils::check_semantic_labels(*mesh);
    }

    SECTION("Simple Cube: Zero geometry")
    {
        Scalar w = 1.0, h = 1.0, d = 1.0, r = 0.0;
        Index n = 1;
        SECTION("w=0")
        {
            w = 0.0;
        }
        SECTION("h=0")
        {
            h = 0.0;
        }
        SECTION("d=0")
        {
            d = 0.0;
        }
        SECTION("w=0,h=0")
        {
            w = 0.0;
            h = 0.0;
        }
        SECTION("w=0,d=0")
        {
            w = 0.0;
            d = 0.0;
        }
        SECTION("h=0,d=0")
        {
            w = 0.0;
            d = 0.0;
        }
        SECTION("w=0,d=0,h=0")
        {
            w = 0.0;
            d = 0.0;
            h = 0.0;
        }

        auto mesh = lagrange::primitive::generate_rounded_cube<MeshType>(w, h, d, r, n);
        REQUIRE(mesh->get_vertices().hasNaN() == false);
    }

    SECTION("Sphere")
    {
        Scalar w = 1.0, h = 1.0, d = 1.0, r = 0.5;
        Index n = 10;
        auto mesh = lagrange::primitive::generate_rounded_cube<MeshType>(w, h, d, r, n);
        primitive_test_utils::validate_primitive(*mesh);
        primitive_test_utils::check_degeneracy(*mesh);
        check_dimension(*mesh, w, h, d);
        primitive_test_utils::check_semantic_labels(*mesh);
    }

    SECTION("config struct")
    {
        lagrange::primitive::RoundedCubeConfig config;
        config.output_normals = false;
        config.center = {0, 0, 0.6f};
        auto mesh = lagrange::primitive::generate_rounded_cube<MeshType>(config);
        REQUIRE(!mesh->has_indexed_attribute("normal"));
        primitive_test_utils::validate_primitive(*mesh);
        primitive_test_utils::check_degeneracy(*mesh);
        primitive_test_utils::check_semantic_labels(*mesh);

        const auto& vertices = mesh->get_vertices();
        REQUIRE(vertices.col(2).minCoeff() > 0);
    }
}
#endif
