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
#include <lagrange/testing/common.h>

#include <lagrange/split_facets_by_material.h>

TEST_CASE("split_facets_by_material", "[core]")
{
    using namespace lagrange;
    using Scalar = double;
    using Index = uint32_t;

    SurfaceMesh<Scalar, Index> mesh(2);
    mesh.add_vertex({0, 0});
    mesh.add_vertex({1, 0});
    mesh.add_vertex({0, 1});
    mesh.add_vertex({1, 1});
    mesh.add_triangle(0, 1, 2);
    mesh.add_triangle(1, 3, 2);

    // clang-format off
    std::vector<Scalar> labels = {
        1.0, 0.0,
        1.0, 0.0,
        0.0, 1.0,
        0.0, 1.0,
    };
    // clang-format on
    mesh.template create_attribute<Scalar>(
        "labels",
        AttributeElement::Vertex,
        2,
        AttributeUsage::Vector,
        {labels.data(), labels.size()});

    split_facets_by_material(mesh, "labels");

    REQUIRE(mesh.get_num_facets() == 6);
}
