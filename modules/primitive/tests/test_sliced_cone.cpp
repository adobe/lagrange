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
#include <lagrange/mesh_cleanup/detect_degenerate_triangles.h>
#include <lagrange/primitive/generate_rounded_cone.h>
#include <lagrange/testing/common.h>
#include <catch2/catch_approx.hpp>

#include "primitive_test_utils.h"

TEST_CASE("SlicedCone", "[primitive][sliced_cone]" LA_SLOW_DEBUG_FLAG)
{
    using namespace lagrange;

    using MeshType = lagrange::TriangleMesh3D;
    using Scalar = MeshType::Scalar;
    using Index = MeshType::Index;

    auto check_dimension = [](MeshType& mesh, const Scalar radius, const Scalar height) {
        const auto& vertices = mesh.get_vertices();
        const auto x_range = vertices.col(0).maxCoeff() - vertices.col(0).minCoeff();
        const auto y_range = vertices.col(1).maxCoeff() - vertices.col(1).minCoeff();
        const auto z_range = vertices.col(2).maxCoeff() - vertices.col(2).minCoeff();
        REQUIRE(x_range <= Catch::Approx(2 * radius));
        REQUIRE(y_range == Catch::Approx(height));
        REQUIRE(z_range <= Catch::Approx(2 * radius));
    };

    SECTION("Sliced cone : Simple")
    {
        Scalar r_top = 0.0, r_bottom = 2.0, h = 5.0, b_top = 0.0, b_bottom = 0.0;
        Index sections = 50, seg_top = 1, seg_bottom = 1;
        Scalar begin_sweep = 0.0, end_sweep = 0.0;

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

        auto mesh = lagrange::primitive::generate_rounded_cone<MeshType>(
            r_top,
            r_bottom,
            h,
            b_top,
            b_bottom,
            sections,
            seg_top,
            seg_bottom,
            begin_sweep,
            end_sweep);
        primitive_test_utils::validate_primitive(*mesh);
        primitive_test_utils::check_degeneracy(*mesh);
        check_dimension(*mesh, r_bottom, h);
    }

    SECTION("Sliced cone: Simple[Sections]")
    {
        Scalar r_top = 0.0, r_bottom = 2.0, h = 5.0, b_top = 0.0, b_bottom = 0.0;
        Index seg_top = 1, seg_bottom = 1;
        Scalar begin_sweep = 0.0, end_sweep = 0.25 * lagrange::internal::pi;
        Index n = 1;
        SECTION("Min Sections")
        {
            n = 3;
        }
        SECTION("Many Sections")
        {
            n = (Index)1e2;
        }

        auto mesh = lagrange::primitive::generate_rounded_cone<MeshType>(
            r_top,
            r_bottom,
            h,
            b_top,
            b_bottom,
            n,
            seg_top,
            seg_bottom,
            begin_sweep,
            end_sweep);
        primitive_test_utils::validate_primitive(*mesh);
        primitive_test_utils::check_degeneracy(*mesh);
        check_dimension(*mesh, r_bottom, h);
    }

    SECTION("Sliced cone: RoundedBottom")
    {
        Scalar r_top = 0.0, r_bottom = 2.0, h = 5.0, b_top = 0.0, b_bottom = 0.5;
        Index sections = 50, seg_top = 1, seg_bottom = 20;
        Scalar begin_sweep = 0.0, end_sweep = 0.0;

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

        auto mesh = lagrange::primitive::generate_rounded_cone<MeshType>(
            r_top,
            r_bottom,
            h,
            b_top,
            b_bottom,
            sections,
            seg_top,
            seg_bottom,
            begin_sweep,
            end_sweep);
        primitive_test_utils::validate_primitive(*mesh);
        primitive_test_utils::check_degeneracy(*mesh);
        check_dimension(*mesh, r_bottom, h);
    }

    SECTION("Sliced cone: RoundedBottom[Sections]")
    {
        Scalar r_top = 0.0, r_bottom = 2.0, h = 5.0, b_top = 0.0, b_bottom = 0.5;
        Index seg_top = 1, seg_bottom = 20;
        Scalar begin_sweep = 0.0, end_sweep = 0.25 * lagrange::internal::pi;
        Index n = 1;
        SECTION("Min Sections")
        {
            n = 3;
        }
        SECTION("Many Sections")
        {
            n = (Index)1e2;
        }

        auto mesh = lagrange::primitive::generate_rounded_cone<MeshType>(
            r_top,
            r_bottom,
            h,
            b_top,
            b_bottom,
            n,
            seg_top,
            seg_bottom,
            begin_sweep,
            end_sweep);
        primitive_test_utils::validate_primitive(*mesh);
        primitive_test_utils::check_degeneracy(*mesh);
        check_dimension(*mesh, r_bottom, h);
    }

    SECTION("Sliced cone: RoundedTopAndBottom")
    {
        Scalar r_top = 2.0, r_bottom = 3.0, h = 5.0, b_top = 1, b_bottom = 1;
        Index sections = 50, seg_top = 1, seg_bottom = 20;
        Scalar begin_sweep = 0.0, end_sweep = 0.0;

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

        auto mesh = lagrange::primitive::generate_rounded_cone<MeshType>(
            r_top,
            r_bottom,
            h,
            b_top,
            b_bottom,
            sections,
            seg_top,
            seg_bottom,
            begin_sweep,
            end_sweep);
        primitive_test_utils::validate_primitive(*mesh);
        primitive_test_utils::check_degeneracy(*mesh);
        check_dimension(*mesh, r_bottom, h);
    }

    SECTION("Sliced cone: RoundedTopAndBottom[Sections]")
    {
        Scalar r_top = 2.0, r_bottom = 3.0, h = 5.0, b_top = 1, b_bottom = 1;
        Index seg_top = 1, seg_bottom = 20;
        Scalar begin_sweep = 0.0, end_sweep = 0.25 * lagrange::internal::pi;
        Index n = 1;
        SECTION("Min Sections")
        {
            n = 3;
        }
        SECTION("Many Sections")
        {
            n = (Index)1e2;
        }

        auto mesh = lagrange::primitive::generate_rounded_cone<MeshType>(
            r_top,
            r_bottom,
            h,
            b_top,
            b_bottom,
            n,
            seg_top,
            seg_bottom,
            begin_sweep,
            end_sweep);
        primitive_test_utils::validate_primitive(*mesh);
        primitive_test_utils::check_degeneracy(*mesh);
        check_dimension(*mesh, r_bottom, h);
    }
}
