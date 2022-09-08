/*
 * Copyright 2017 Adobe. All rights reserved.
 * This file is licensed to you under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License. You may obtain a copy
 * of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under
 * the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
 * OF ANY KIND, either express or implied. See the License for the specific language
 * governing permissions and limitations under the License.
 */
#include <Eigen/Core>
#include <lagrange/testing/common.h>

#include <lagrange/Components.h>
#include <lagrange/Connectivity.h>
#include <lagrange/Mesh.h>
#include <lagrange/common.h>
#include <lagrange/create_mesh.h>

TEST_CASE("Components", "[components][simple]")
{
    using namespace lagrange;

    Vertices3D vertices(3, 3);
    vertices << 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0, 0.0;
    Triangles facets(1, 3);
    facets << 0, 1, 2;

    auto mesh = create_mesh(vertices, facets);
    mesh->initialize_components();

    REQUIRE(mesh->get_num_components() == 1);
    const auto& comp_list = mesh->get_components();
    REQUIRE(comp_list[0].size() == 1);
}

TEST_CASE("ComponentsVertexTouch", "[components][simple]")
{
    using namespace lagrange;

    Vertices3D vertices(5, 3);
    vertices << 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 1.0, 1.0, 0.0, 0.0, 0.0, 1.0;
    Triangles facets(2, 3);
    facets << 0, 1, 2, 0, 3, 4;

    auto mesh = create_mesh(vertices, facets);
    mesh->initialize_components();

    REQUIRE(mesh->get_num_components() == 2);
    const auto& comp_list = mesh->get_components();
    REQUIRE(comp_list[0].size() == 1);
    REQUIRE(comp_list[1].size() == 1);

    const auto& comp_ids = mesh->get_per_facet_component_ids();
    REQUIRE(comp_ids.size() == 2);
}

TEST_CASE("MultiComps", "[components][simple]")
{
    using namespace lagrange;

    Vertices3D vertices(6, 3);
    vertices << 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 1.0, 1.0, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0,
        1.0;
    Triangles facets(2, 3);
    facets << 0, 1, 2, 3, 4, 5;

    auto mesh = create_mesh(vertices, facets);
    mesh->initialize_components();

    REQUIRE(mesh->get_num_components() == 2);
    const auto& comp_list = mesh->get_components();
    REQUIRE(comp_list[0].size() == 1);
    REQUIRE(comp_list[1].size() == 1);

    const auto& comp_ids = mesh->get_per_facet_component_ids();
    REQUIRE(comp_ids.size() == 2);
}
