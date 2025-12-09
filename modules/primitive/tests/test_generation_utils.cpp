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
#ifdef LAGRANGE_ENABLE_LEGACY_FUNCTIONS

    #include <lagrange/common.h>
    #include <lagrange/mesh_cleanup/detect_degenerate_triangles.h>
    #include <lagrange/primitive/legacy/generation_utils.h>
    #include <lagrange/testing/common.h>
    #include <catch2/catch_approx.hpp>

    #include "primitive_test_utils.h"

TEST_CASE("Sweep", "[primitive][sweep]")
{
    using namespace lagrange;

    using MeshType = lagrange::TriangleMesh3D;
    using Scalar = MeshType::Scalar;
    using Index = MeshType::Index;
    using Generator = Eigen::Matrix<MeshType::Scalar, 1, 3>;
    using Generator_function = std::function<Generator(Scalar)>;

    Scalar radius = 1.0;
    Scalar height = 5.0;
    Generator_function cylinder_generator = [&height, &radius](Scalar t) -> Generator {
        Generator vert;
        vert << radius, height * t, 0.0;
        return vert;
    };

    auto check_dimension = [&height, &radius](MeshType& mesh) {
        const auto& vertices = mesh.get_vertices();
        const auto x_range = vertices.col(0).maxCoeff() - vertices.col(0).minCoeff();
        const auto y_range = vertices.col(1).maxCoeff() - vertices.col(1).minCoeff();
        const auto z_range = vertices.col(2).maxCoeff() - vertices.col(2).minCoeff();
        REQUIRE(x_range <= Catch::Approx(2 * radius));
        REQUIRE(y_range == Catch::Approx(height));
        REQUIRE(z_range <= Catch::Approx(2 * radius));
    };

    SECTION("Sweep: Sections")
    {
        Index n;
        Scalar r_top = 1.0;
        Scalar r_bottom = 1.0;

        SECTION("Minimums Sections")
        {
            n = 3;
        }
        SECTION("Many Sections")
        {
            n = (Index)1e2;
        }
        auto mesh = lagrange::sweep<MeshType>(
            lagrange::generate_profile<MeshType>(cylinder_generator, 20),
            n,
            r_top,
            r_bottom);
        primitive_test_utils::validate_primitive(*mesh, 1);
        primitive_test_utils::check_degeneracy(*mesh);
        check_dimension(*mesh);
    }
}

TEST_CASE("Disk", "[primitive][disk]")
{
    using namespace lagrange;

    using MeshType = lagrange::TriangleMesh3D;
    using Scalar = MeshType::Scalar;
    using Index = MeshType::Index;
    Scalar radius = 2;

    auto check_dimension = [&radius](MeshType& mesh) {
        const auto& vertices = mesh.get_vertices();
        const auto x_range = vertices.col(0).maxCoeff() - vertices.col(0).minCoeff();
        const auto z_range = vertices.col(2).maxCoeff() - vertices.col(2).minCoeff();
        REQUIRE(x_range <= Catch::Approx(2 * radius));
        REQUIRE(z_range <= Catch::Approx(2 * radius));
    };


    SECTION("Generate Disk: Sections")
    {
        Index n;

        SECTION("Minimums Sections")
        {
            n = 3;
        }
        SECTION("Many Sections")
        {
            n = (Index)1e2;
        }
        auto mesh = lagrange::generate_disk<MeshType>(radius, n);
        primitive_test_utils::validate_primitive(*mesh, 1);
        primitive_test_utils::check_degeneracy(*mesh);
        check_dimension(*mesh);
    }
}

#endif
