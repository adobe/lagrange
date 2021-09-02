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
#include <lagrange/testing/common.h>

#include <lagrange/common.h>
#include <lagrange/create_mesh.h>
#include <lagrange/mesh_cleanup/resolve_vertex_nonmanifoldness.h>


TEST_CASE("resolve_vertex_nonmanifoldness", "[nonmanifold][Mesh][cleanup]")
{
    using namespace lagrange;

    SECTION("simple")
    {
        Eigen::Matrix<double, Eigen::Dynamic, 3> vertices(3, 3);
        vertices << 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0, 0.0;
        Eigen::Matrix<int, Eigen::Dynamic, 3> facets(1, 3);
        facets << 0, 1, 2;

        auto in_mesh = lagrange::create_mesh(vertices, facets);
        in_mesh->initialize_topology();
        REQUIRE(in_mesh->is_vertex_manifold());

        in_mesh->initialize_connectivity();

        auto out_mesh = lagrange::resolve_vertex_nonmanifoldness(*in_mesh);
        out_mesh->initialize_topology();
        REQUIRE(out_mesh->is_vertex_manifold());
    }

    SECTION("two triangles")
    {
        Eigen::Matrix<double, Eigen::Dynamic, 3> vertices(4, 3);
        vertices << 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0;
        Eigen::Matrix<int, Eigen::Dynamic, 3> facets(2, 3);
        facets << 0, 1, 2, 1, 0, 3;

        auto in_mesh = lagrange::create_mesh(vertices, facets);
        in_mesh->initialize_topology();
        REQUIRE(in_mesh->is_vertex_manifold());

        in_mesh->initialize_connectivity();

        auto out_mesh = lagrange::resolve_vertex_nonmanifoldness(*in_mesh);
        out_mesh->initialize_topology();
        REQUIRE(out_mesh->is_vertex_manifold());
    }

    // TODO: probably needs more...
}
