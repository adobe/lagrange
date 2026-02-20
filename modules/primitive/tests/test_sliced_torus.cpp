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
#include <lagrange/common.h>
#include <lagrange/primitive/generate_torus.h>
#include <lagrange/testing/common.h>
#include <catch2/catch_approx.hpp>

#include "primitive_test_utils.h"

TEST_CASE("SlicedTorus", "[primitive][sliced_torus]" LA_SLOW_DEBUG_FLAG)
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

    SECTION("Sliced Torus : Simple")
    {
        Scalar r_major = 0.5, r_minor = 0.05;
        Index ring_segments = 50, pipe_segments = 50;
        Scalar begin_sweep = 0.0, end_sweep = 0.0;
        auto center = Eigen::Matrix<typename MeshType::Scalar, 3, 1>(0.0, 0.0, 0.0);

        end_sweep = 2 * lagrange::internal::pi;
        SECTION("Begin Sweep: 0.0")
        {
            begin_sweep = 0.0;
        }
        SECTION("Begin Sweep: Quadrant 1")
        {
            begin_sweep = 0.25 * lagrange::internal::pi;
        }
        SECTION("Begin Sweep: Quadrant 2")
        {
            begin_sweep = 0.3 * lagrange::internal::pi;
        }
        SECTION("Begin Sweep: Quadrant 3")
        {
            begin_sweep = 1.25 * lagrange::internal::pi;
        }
        SECTION("Begin Sweep: Quadrant 4")
        {
            begin_sweep = 1.6 * lagrange::internal::pi;
        }

        begin_sweep = 0.0;
        SECTION("End Sweep: 2*lagrange::internal::pi")
        {
            end_sweep = 2 * lagrange::internal::pi;
        }
        SECTION("End Sweep: Quadrant 1")
        {
            end_sweep = 0.25 * lagrange::internal::pi;
        }
        SECTION("End Sweep: Quadrant 2")
        {
            end_sweep = 0.3 * lagrange::internal::pi;
        }
        SECTION("End Sweep: Quadrant 3")
        {
            end_sweep = 1.25 * lagrange::internal::pi;
        }
        SECTION("End Sweep: Quadrant 4")
        {
            end_sweep = 1.6 * lagrange::internal::pi;
        }
        auto mesh = lagrange::primitive::generate_torus<MeshType>(
            r_major,
            r_minor,
            ring_segments,
            pipe_segments,
            center,
            begin_sweep,
            end_sweep);
        primitive_test_utils::validate_primitive(*mesh);
        primitive_test_utils::check_degeneracy(*mesh);
        check_dimension(*mesh, r_major, r_minor);
    }

    SECTION("Sliced Torus : Simple[Ring]")
    {
        Scalar r_major = 0.5, r_minor = 0.05;
        Index pipe_segments = 50;
        Index n = 1;
        Scalar begin_sweep = 0.0, end_sweep = 0.25 * lagrange::internal::pi;
        auto center = Eigen::Matrix<typename MeshType::Scalar, 3, 1>(0.0, 0.0, 0.0);

        SECTION("Min ring segments")
        {
            n = 3;
        }
        SECTION("Many ring segments")
        {
            n = (Index)1e2;
        }
        auto mesh = lagrange::primitive::generate_torus<MeshType>(
            r_major,
            r_minor,
            n,
            pipe_segments,
            center,
            begin_sweep,
            end_sweep);
        primitive_test_utils::validate_primitive(*mesh);
        primitive_test_utils::check_degeneracy(*mesh);
        check_dimension(*mesh, r_major, r_minor);
    }

    SECTION("Sliced Torus : Simple[Pipe]")
    {
        Scalar r_major = 0.5, r_minor = 0.05;
        Index ring_segments = 50;
        Index n = 1;
        Scalar begin_sweep = 0.0, end_sweep = 0.25 * lagrange::internal::pi;
        auto center = Eigen::Matrix<typename MeshType::Scalar, 3, 1>(0.0, 0.0, 0.0);

        SECTION("Min pipe segments")
        {
            n = 3;
        }
        SECTION("Many pipe segments")
        {
            n = (Index)1e2;
        }
        auto mesh = lagrange::primitive::generate_torus<MeshType>(
            r_major,
            r_minor,
            ring_segments,
            n,
            center,
            begin_sweep,
            end_sweep);
        primitive_test_utils::validate_primitive(*mesh);
        primitive_test_utils::check_degeneracy(*mesh);
        check_dimension(*mesh, r_major, r_minor);
    }
}
