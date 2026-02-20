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
#include <lagrange/io/save_mesh.h>
#include <lagrange/primitive/generate_hexahedron.h>
#include <lagrange/primitive/generate_octahedron.h>
#include <lagrange/primitive/generate_subdivided_sphere.h>
#include <lagrange/testing/common.h>
#include <catch2/catch_approx.hpp>
#include "primitive_test_utils.h"

template <typename MeshType>
void check_dimension(const MeshType& mesh, typename MeshType::Scalar radius)
{
    const auto& vertices = mesh.get_vertices();
    const auto x_range = vertices.col(0).maxCoeff() - vertices.col(0).minCoeff();
    const auto y_range = vertices.col(1).maxCoeff() - vertices.col(1).minCoeff();
    const auto z_range = vertices.col(2).maxCoeff() - vertices.col(2).minCoeff();
    REQUIRE(x_range <= Catch::Approx(2 * radius));
    REQUIRE(y_range <= Catch::Approx(2 * radius));
    REQUIRE(z_range <= Catch::Approx(2 * radius));
}

TEST_CASE("SubdividedSphere", "[primitive][subdivided_sphere]")
{
    using namespace lagrange;

    SECTION("SubdividedSphere: MeshTypes")
    {
        SECTION("Triangle Mesh")
        {
            using MeshType = lagrange::TriangleMesh3D;
            MeshType::Index n = 1;
            MeshType::Scalar r = 2.0;
            auto base_shape = lagrange::primitive::generate_octahedron<MeshType>(r);
            auto subdiv_mesh = lagrange::primitive::generate_subdivided_sphere<MeshType>(
                *base_shape,
                r,
                {0, 0, 0},
                n);
            check_dimension(*subdiv_mesh, r);
            primitive_test_utils::check_semantic_labels(*subdiv_mesh);
            primitive_test_utils::validate_primitive(*subdiv_mesh);
            primitive_test_utils::check_degeneracy(*subdiv_mesh);
        }

        SECTION("Quad Mesh")
        {
            using MeshType = lagrange::QuadMesh3D;
            MeshType::Index n = 1;
            MeshType::Scalar r = 2.0;
            auto base_shape = lagrange::primitive::generate_hexahedron<MeshType>(r);
            auto subdiv_mesh = lagrange::primitive::generate_subdivided_sphere<MeshType>(
                *base_shape,
                r,
                {0, 0, 0},
                n);
            check_dimension(*subdiv_mesh, r);
            primitive_test_utils::check_semantic_labels(*subdiv_mesh);
        }
    }

    SECTION("SubdividedSphere: Subdivisions Trimesh")
    {
        using MeshType = lagrange::TriangleMesh3D;
        MeshType::Scalar r = 2.0;
        MeshType::Index n;
        SECTION("Zero Subdivisions")
        {
            n = 0;
        }
        SECTION("Min Subdivisions")
        {
            n = 1;
        }
        SECTION("Many Subdivisions")
        {
            n = 4;
        }
        auto base_shape = lagrange::primitive::generate_octahedron<MeshType>(r);
        auto subdiv_mesh =
            lagrange::primitive::generate_subdivided_sphere<MeshType>(*base_shape, r, {0, 0, 0}, n);
        check_dimension(*subdiv_mesh, r);
        primitive_test_utils::check_semantic_labels(*subdiv_mesh);
        primitive_test_utils::validate_primitive(*subdiv_mesh);
        primitive_test_utils::check_degeneracy(*subdiv_mesh);
    }

    SECTION("SubdividedSphere: Subdivisions QuadMesh")
    {
        using MeshType = lagrange::QuadMesh3D;
        MeshType::Scalar r = 2.0;
        MeshType::Index n;
        SECTION("Zero Subdivisions")
        {
            n = 0;
        }
        SECTION("Min Subdivisions")
        {
            n = 1;
        }
        SECTION("Many Subdivisions")
        {
            n = 4;
        }
        auto base_shape = lagrange::primitive::generate_hexahedron<MeshType>(r);
        auto subdiv_mesh =
            lagrange::primitive::generate_subdivided_sphere<MeshType>(*base_shape, r, {0, 0, 0}, n);
        check_dimension(*subdiv_mesh, r);
        primitive_test_utils::check_semantic_labels(*subdiv_mesh);
    }

    SECTION("Non-origin center")
    {
        using MeshType = lagrange::TriangleMesh3D;
        MeshType::Scalar r = 2.0;
        MeshType::Index n = 5;
        auto base_shape = lagrange::primitive::generate_octahedron<MeshType>(r);
        auto subdiv_mesh = lagrange::primitive::generate_subdivided_sphere<MeshType>(
            *base_shape,
            r,
            {10, 10, 10},
            n);
        check_dimension(*subdiv_mesh, r);
        primitive_test_utils::check_semantic_labels(*subdiv_mesh);
        primitive_test_utils::validate_primitive(*subdiv_mesh);
        primitive_test_utils::check_degeneracy(*subdiv_mesh);
    }

    SECTION("Zero radius")
    {
        using MeshType = lagrange::TriangleMesh3D;
        MeshType::Scalar r = 0.0;
        MeshType::Index n = 3;
        auto base_shape = lagrange::primitive::generate_octahedron<MeshType>(r);
        auto subdiv_mesh = lagrange::primitive::generate_subdivided_sphere<MeshType>(
            *base_shape,
            r,
            {10.0, 10.0, 10.0},
            n);
        REQUIRE(subdiv_mesh->get_vertices().hasNaN() == false);
        REQUIRE(subdiv_mesh->get_vertices().isConstant(10.0));
    }

    SECTION("Invalid dimension")
    {
        using MeshType = lagrange::TriangleMesh3D;
        MeshType::Scalar r = -2.0;
        LA_REQUIRE_THROWS(lagrange::primitive::generate_hexahedron<MeshType>(r));
    }
}
