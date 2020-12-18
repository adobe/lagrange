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
#include <cmath>

#include <lagrange/Mesh.h>
#include <lagrange/common.h>
#include <lagrange/compute_vertex_normal.h>
#include <lagrange/create_mesh.h>

TEST_CASE("ComputeVertexNormal", "[mesh][triangle][attribute][vertex_normal]")
{
    using namespace lagrange;
    auto mesh = create_cube();
    const size_t num_vertices = mesh->get_num_vertices();
    compute_vertex_normal(*mesh);
    REQUIRE(mesh->has_vertex_attribute("normal"));

    const auto& vertex_normals = mesh->get_vertex_attribute("normal");
    REQUIRE(safe_cast<size_t>(vertex_normals.rows()) == num_vertices);
    REQUIRE(vertex_normals.cols() == 3);

    for (size_t i = 0; i < num_vertices; i++) {
        REQUIRE(std::abs(vertex_normals(i, 0)) == Approx(1.0 / sqrt(3.0)));
        REQUIRE(std::abs(vertex_normals(i, 1)) == Approx(1.0 / sqrt(3.0)));
        REQUIRE(std::abs(vertex_normals(i, 2)) == Approx(1.0 / sqrt(3.0)));
    }
}
