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
#include <lagrange/primitive/generate_torus.h>
#include <lagrange/testing/common.h>
#include <catch2/catch_approx.hpp>
#include "primitive_test_utils.h"

TEST_CASE("generate_torus", "[primitive][surface]")
{
    using namespace lagrange;
    using Scalar = float;
    using Index = uint32_t;

    primitive::TorusOptions setting;

    SECTION("Simple torus")
    {
        setting.triangulate = true;
        auto mesh = primitive::generate_torus<Scalar, Index>(setting);
        primitive_test_utils::validate_primitive(mesh);
        primitive_test_utils::check_degeneracy(mesh);
        primitive_test_utils::check_UV(mesh);
    }

    SECTION("Partial torus")
    {
        setting.ring_segments = 7;
        setting.pipe_segments = 11;
        setting.start_sweep_angle = static_cast<Scalar>(lagrange::internal::pi / 6);
        setting.end_sweep_angle = static_cast<Scalar>(3 * lagrange::internal::pi / 2);
        auto mesh = primitive::generate_torus<Scalar, Index>(setting);
        primitive_test_utils::validate_primitive(mesh);
        primitive_test_utils::check_degeneracy(mesh);
        primitive_test_utils::check_UV(mesh);
    }

    SECTION("Open torus")
    {
        setting.pipe_segments = 3;
        setting.ring_segments = 3;
        setting.start_sweep_angle = static_cast<Scalar>(0);
        setting.end_sweep_angle = static_cast<Scalar>(lagrange::internal::pi);
        setting.with_top_cap = false;
        setting.with_bottom_cap = false;
        auto mesh = primitive::generate_torus<Scalar, Index>(setting);
        primitive_test_utils::validate_primitive(mesh, 2);
        primitive_test_utils::check_degeneracy(mesh);
        primitive_test_utils::check_UV(mesh);
    }

    SECTION("Low poly")
    {
        setting.major_radius = 1;
        setting.minor_radius = 0.1;
        setting.pipe_segments = 3;
        setting.ring_segments = 3;
        auto mesh = primitive::generate_torus<Scalar, Index>(setting);
        REQUIRE(mesh.get_num_vertices() == 9);
        primitive_test_utils::validate_primitive(mesh);
        primitive_test_utils::check_degeneracy(mesh);
        primitive_test_utils::check_UV(mesh);
    }

    SECTION("Zero radius")
    {
        setting.major_radius = 0;
        setting.minor_radius = 0;
        setting.pipe_segments = 3;
        setting.ring_segments = 3;
        setting.fixed_uv = true;
        auto mesh = primitive::generate_torus<Scalar, Index>(setting);
        REQUIRE(mesh.get_num_vertices() == 9);
        primitive_test_utils::validate_primitive(mesh);
    }
}

#ifdef LAGRANGE_ENABLE_LEGACY_FUNCTIONS

TEST_CASE("Torus", "[primitive][torus]" LA_SLOW_DEBUG_FLAG)
{
    using namespace lagrange;

    using MeshType = lagrange::TriangleMesh3D;
    using Scalar = MeshType::Scalar;
    using Index = MeshType::Index;

    auto check_dimension =
        [](MeshType& mesh, const Scalar major_radius, const Scalar minor_radius) {
            const auto& vertices = mesh.get_vertices();
            const auto x_range = vertices.col(0).maxCoeff() - vertices.col(0).minCoeff();
            const auto y_range = vertices.col(1).maxCoeff() - vertices.col(1).minCoeff();
            const auto z_range = vertices.col(2).maxCoeff() - vertices.col(2).minCoeff();
            REQUIRE(x_range <= Catch::Approx(2 * (major_radius + 2 * minor_radius)));
            REQUIRE(y_range <= Catch::Approx(2 * minor_radius));
            REQUIRE(z_range <= Catch::Approx(2 * (major_radius + 2 * minor_radius)));
        };

    SECTION("Simple Torus : Ring Segments")
    {
        Scalar r_major = 0.5, r_minor = 0.05;
        Index pipe_segments = 50;
        Index n = 1;
        SECTION("Min ring segments")
        {
            n = 3;
        }
        SECTION("Many ring segments")
        {
            n = 1e2;
        }
        auto mesh =
            lagrange::primitive::generate_torus<MeshType>(r_major, r_minor, n, pipe_segments);
        primitive_test_utils::validate_primitive(*mesh);
        primitive_test_utils::check_degeneracy(*mesh);
        check_dimension(*mesh, r_major, r_minor);
        primitive_test_utils::check_semantic_labels(*mesh);
    }

    SECTION("Simple Torus : Pipe Segments")
    {
        Scalar r_major = 0.5, r_minor = 0.05;
        Index ring_segments = 50;
        Index n = 1;
        SECTION("Min pipe segments")
        {
            n = 3;
        }
        SECTION("Many pipe segments")
        {
            n = 1e2;
        }
        auto mesh =
            lagrange::primitive::generate_torus<MeshType>(r_major, r_minor, ring_segments, n);
        primitive_test_utils::validate_primitive(*mesh);
        primitive_test_utils::check_degeneracy(*mesh);
        check_dimension(*mesh, r_major, r_minor);
        primitive_test_utils::check_semantic_labels(*mesh);
    }

    SECTION("Simple Torus : Comparison to 2*lagrange::internal::pi and slice")
    {
        Scalar r_major = 0.5, r_minor = 0.05;
        Index ring_segments = 50;
        Index n = 5;
        Eigen::Matrix<Scalar, 1, 3> center = {0, 0, 0};
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
        auto mesh = lagrange::primitive::generate_torus<MeshType>(
            r_major,
            r_minor,
            ring_segments,
            n,
            center,
            begin_angle,
            sweep_angle);
        primitive_test_utils::validate_primitive(*mesh);
        primitive_test_utils::check_degeneracy(*mesh);
        check_dimension(*mesh, r_major, r_minor);
        primitive_test_utils::check_semantic_labels(*mesh);
    }

    SECTION("Simple Torus: Zero geometry")
    {
        Scalar r_major = 0.5, r_minor = 0.05;
        Index pipe_segments = 50;
        Index ring_segments = 50;
        SECTION("r_major=0")
        {
            r_major = 0.0;
            r_minor = 0.0;
        }
        SECTION("r_minor=0")
        {
            r_minor = 0.0;
        }
        auto mesh = lagrange::primitive::generate_torus<MeshType>(
            r_major,
            r_minor,
            ring_segments,
            pipe_segments);
        REQUIRE(mesh->get_vertices().hasNaN() == false);
    }

    SECTION("Invalid dimension")
    {
        Scalar r_major = -0.25, r_minor = 0.5;
        Index ring_segments = 0, pipe_segments = 50;
        auto mesh = lagrange::primitive::generate_torus<MeshType>(
            r_major,
            r_minor,
            ring_segments,
            pipe_segments);
        check_dimension(*mesh, 0, r_minor);
    }
}

#endif
