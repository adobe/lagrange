/*
 * Copyright 2025 Adobe. All rights reserved.
 * This file is licensed to you under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License. You may obtain a copy
 * of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under
 * the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
 * OF ANY KIND, either express or implied. See the License for the specific language
 * governing permissions and limitations under the License.
 */

#include <lagrange/compute_facet_circumcenter.h>
#include <lagrange/testing/common.h>
#include <lagrange/views.h>

#include <catch2/matchers/catch_matchers_floating_point.hpp>

TEST_CASE("compute_facet_circumcenter", "[core][surface]")
{
    using namespace lagrange;
    using Scalar = double;
    using Index = uint32_t;

    auto mesh = lagrange::testing::load_surface_mesh<Scalar, Index>("open/core/stanford-bunny.obj");
    auto id = lagrange::compute_facet_circumcenter(mesh);
    auto center = attribute_matrix_view<Scalar>(mesh, id);

    Index num_facets = mesh.get_num_facets();
    auto vertices = vertex_view(mesh);
    for (Index fid = 0; fid < num_facets; fid++) {
        auto f = mesh.get_facet_vertices(fid);
        auto c = center.row(fid).eval();
        auto v0 = vertices.row(f[0]);
        auto v1 = vertices.row(f[1]);
        auto v2 = vertices.row(f[2]);

        Scalar r0 = (c - v0).norm();
        Scalar r1 = (c - v1).norm();
        Scalar r2 = (c - v2).norm();
        REQUIRE_THAT(r0, Catch::Matchers::WithinAbs(r1, 1e-6));
        REQUIRE_THAT(r0, Catch::Matchers::WithinAbs(r2, 1e-6));
    }
}
