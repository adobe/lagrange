/*
 * Copyright 2024 Adobe. All rights reserved.
 * This file is licensed to you under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License. You may obtain a copy
 * of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under
 * the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
 * OF ANY KIND, either express or implied. See the License for the specific language
 * governing permissions and limitations under the License.
 */

#include <lagrange/reorder_mesh.h>
#include <lagrange/testing/common.h>
#include <lagrange/views.h>

TEST_CASE("reorder_mesh", "[core][reorder_mesh]")
{
    auto mesh = lagrange::testing::load_surface_mesh<double, uint32_t>("open/core/dragon.obj");

    auto mesh1 = mesh;
    lagrange::reorder_mesh(mesh1, lagrange::ReorderingMethod::Morton);

    auto mesh2 = mesh;
    lagrange::reorder_mesh(mesh2, lagrange::ReorderingMethod::Morton);

    REQUIRE(vertex_view(mesh1) == vertex_view(mesh2));
    REQUIRE(facet_view(mesh1) == facet_view(mesh2));

    auto mesh3 = mesh;
    lagrange::reorder_mesh(mesh3, lagrange::ReorderingMethod::Hilbert);

    REQUIRE(vertex_view(mesh1) != vertex_view(mesh3));
    REQUIRE(facet_view(mesh1) != facet_view(mesh3));

    auto mesh4 = mesh;
    lagrange::reorder_mesh(mesh4, lagrange::ReorderingMethod::Hilbert);
    REQUIRE(vertex_view(mesh3) == vertex_view(mesh4));
    REQUIRE(facet_view(mesh3) == facet_view(mesh4));
}
