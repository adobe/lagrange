/*
 * Copyright 2023 Adobe. All rights reserved.
 * This file is licensed to you under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License. You may obtain a copy
 * of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under
 * the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
 * OF ANY KIND, either express or implied. See the License for the specific language
 * governing permissions and limitations under the License.
 */
#include <lagrange/testing/common.h>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <lagrange/compute_dihedral_angles.h>
#include <lagrange/internal/constants.h>
#include <lagrange/views.h>

TEST_CASE("compute_dihedral_angles", "[core][surface][utilities]")
{
    using namespace lagrange;
    using Scalar = double;
    using Index = uint32_t;
    constexpr Scalar eps = std::numeric_limits<Scalar>::epsilon();

    SurfaceMesh<Scalar, Index> mesh;

    SECTION("Single triangle")
    {
        mesh.add_vertex({0, 0, 0});
        mesh.add_vertex({1, 0, 0});
        mesh.add_vertex({0, 1, 0});
        mesh.add_triangle(0, 1, 2);

        auto id = compute_dihedral_angles(mesh);
        auto dihedral_angles = attribute_matrix_view<Scalar>(mesh, id);
        REQUIRE_THAT(dihedral_angles(0), Catch::Matchers::WithinAbs(0, eps));
        REQUIRE_THAT(dihedral_angles(1), Catch::Matchers::WithinAbs(0, eps));
        REQUIRE_THAT(dihedral_angles(2), Catch::Matchers::WithinAbs(0, eps));
    }

    SECTION("Two triangles: flat")
    {
        mesh.add_vertex({0, 0, 0});
        mesh.add_vertex({1, 0, 0});
        mesh.add_vertex({0, 1, 0});
        mesh.add_vertex({0, -1, 0});
        mesh.add_triangle(0, 1, 2);
        mesh.add_triangle(1, 0, 3);

        auto id = compute_dihedral_angles(mesh);
        auto dihedral_angles = attribute_matrix_view<Scalar>(mesh, id);
        REQUIRE_THAT(dihedral_angles.minCoeff(), Catch::Matchers::WithinAbs(0, eps));
        REQUIRE_THAT(dihedral_angles.maxCoeff(), Catch::Matchers::WithinAbs(0, eps));
    }

    SECTION("Two triangles: 90 degrees")
    {
        mesh.add_vertex({0, 0, 0});
        mesh.add_vertex({1, 0, 0});
        mesh.add_vertex({0, 1, 0});
        mesh.add_vertex({0, 0, 1});
        mesh.add_triangle(0, 1, 2);
        mesh.add_triangle(1, 0, 3);

        auto id = compute_dihedral_angles(mesh);
        auto dihedral_angles = attribute_matrix_view<Scalar>(mesh, id);
        REQUIRE_THAT(dihedral_angles.minCoeff(), Catch::Matchers::WithinAbs(0, eps));
        REQUIRE_THAT(
            dihedral_angles.maxCoeff(),
            Catch::Matchers::WithinAbs(lagrange::internal::pi / 2, eps));
    }

    SECTION("Two triangles: 180 degrees")
    {
        mesh.add_vertex({0, 0, 0});
        mesh.add_vertex({1, 0, 0});
        mesh.add_vertex({0, 1, 0});
        mesh.add_vertex({0, 1, 0});
        mesh.add_triangle(0, 1, 2);
        mesh.add_triangle(1, 0, 3);

        auto id = compute_dihedral_angles(mesh);
        auto dihedral_angles = attribute_matrix_view<Scalar>(mesh, id);
        REQUIRE_THAT(dihedral_angles.minCoeff(), Catch::Matchers::WithinAbs(0, eps));
        REQUIRE_THAT(
            dihedral_angles.maxCoeff(),
            Catch::Matchers::WithinAbs(lagrange::internal::pi, eps));
    }

    SECTION("Non-manifold")
    {
        mesh.add_vertex({0, 0, 0});
        mesh.add_vertex({1, 0, 0});
        mesh.add_vertex({0, 1, 0});
        mesh.add_vertex({0, 0, 1});
        mesh.add_vertex({1, 1, 1});
        mesh.add_triangle(0, 1, 2);
        mesh.add_triangle(1, 0, 3);
        mesh.add_triangle(0, 1, 4);

        auto id = compute_dihedral_angles(mesh);
        auto dihedral_angles = attribute_matrix_view<Scalar>(mesh, id);
        REQUIRE_THAT(dihedral_angles.minCoeff(), Catch::Matchers::WithinAbs(0, eps));
        REQUIRE_THAT(
            dihedral_angles.maxCoeff(),
            Catch::Matchers::WithinAbs(2 * lagrange::internal::pi, eps));
        REQUIRE((dihedral_angles.array() > lagrange::internal::pi).count() == 1);
    }
}
