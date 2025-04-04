/*
 * Copyright 2022 Adobe. All rights reserved.
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

#include <lagrange/normalize_meshes.h>

TEST_CASE("normalize_meshes", "[surface][utilities]")
{
    using namespace lagrange;
    using Scalar = double;
    using Index = uint32_t;

    SECTION("Single quad")
    {
        lagrange::SurfaceMesh<Scalar, Index> mesh;
        mesh.add_vertex({0, 0, 0});
        mesh.add_vertex({2, 0, 0});
        mesh.add_vertex({2, 2, 0});
        mesh.add_vertex({0, 2, 0});
        mesh.add_quad(0, 1, 2, 3);

        normalize_mesh(mesh);
        for (Index vi = 0; vi < 4; vi++) {
            auto v = mesh.get_position(vi);
            REQUIRE(v[0] >= -1);
            REQUIRE(v[0] <= 1);
            REQUIRE(v[1] >= -1);
            REQUIRE(v[1] <= 1);
            REQUIRE(v[2] >= -1);
            REQUIRE(v[2] <= 1);
        }
    }

    SECTION("Single quad with transform")
    {
        lagrange::SurfaceMesh<Scalar, Index> mesh;
        mesh.add_vertex({0, 0, 1});
        mesh.add_vertex({2, 0, 1});
        mesh.add_vertex({2, 2, 1});
        mesh.add_vertex({0, 2, 1});
        mesh.add_quad(0, 1, 2, 3);

        const auto transform = normalize_mesh_with_transform<3>(mesh);
        for (Index vi = 0; vi < 4; vi++) {
            auto v = mesh.get_position(vi);
            REQUIRE(v[0] >= -1);
            REQUIRE(v[0] <= 1);
            REQUIRE(v[1] >= -1);
            REQUIRE(v[1] <= 1);
            REQUIRE(v[2] >= -1);
            REQUIRE(v[2] <= 1);
        }

        auto transform_ = Eigen::Transform<Scalar, 3, Eigen::Affine>::Identity();
        transform_.translation().array() = 1.0;
        transform_.linear().diagonal().array() = sqrt(2.0);

        const auto transform_error = (transform.matrix() - transform_.matrix()).array().abs().sum();
        REQUIRE(transform_error < 1e-7);
    }

    SECTION("One quad and one triangle")
    {
        lagrange::SurfaceMesh<Scalar, Index> mesh;
        mesh.add_vertex({0, 0, 0});
        mesh.add_vertex({2, 0, 0});
        mesh.add_vertex({2, 2, 0});
        mesh.add_vertex({0, 2, 0});
        mesh.add_vertex({3, 1, 0});
        mesh.add_quad(0, 1, 2, 3);
        mesh.add_triangle(4, 2, 3);

        normalize_mesh(mesh);
        for (Index vi = 0; vi < 4; vi++) {
            auto v = mesh.get_position(vi);
            REQUIRE(v[0] >= -1);
            REQUIRE(v[0] <= 1);
            REQUIRE(v[1] >= -1);
            REQUIRE(v[1] <= 1);
            REQUIRE(v[2] >= -1);
            REQUIRE(v[2] <= 1);
        }
    }
}
