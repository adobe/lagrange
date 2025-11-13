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
#include <lagrange/IndexedAttribute.h>
#include <lagrange/split_facets_by_material.h>
#include <lagrange/testing/common.h>
#include <lagrange/views.h>

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

    SECTION("Simple")
    {
        split_facets_by_material(mesh, "labels");

        REQUIRE(mesh.get_num_facets() == 6);
    }

    SECTION("With UV")
    {
        {
            // clang-format off
            Scalar uv_values[] = {
                0.0, 0.0, // Vertex 0
                1.0, 0.0, // Vertex 1
                0.0, 1.0, // Vertex 2
                1.0, 1.0, // Vertex 3
            };
            Index uv_indices[] = {
                0, 1, 2, // Triangle 0
                1, 3, 2, // Triangle 1
            };
            // clang-format on
            mesh.template create_attribute<Scalar>(
                "uv",
                AttributeElement::Indexed,
                2,
                AttributeUsage::UV,
                {uv_values, 8},
                {uv_indices, 6});
        }

        split_facets_by_material(mesh, "labels");
        REQUIRE(mesh.get_num_facets() == 6);
        REQUIRE(mesh.has_attribute("uv"));
        REQUIRE(mesh.is_attribute_indexed("uv"));

        auto vertices = vertex_view(mesh);
        auto facets = facet_view(mesh);
        const auto& uv_attr = mesh.template get_indexed_attribute<Scalar>("uv");
        auto uv_values = matrix_view(uv_attr.values());
        auto uv_indices = reshaped_view(uv_attr.indices(), 3);

        const Index num_facets = mesh.get_num_facets();
        for (Index i = 0; i < num_facets; i++) {
            for (Index j = 0; j < 3; j++) {
                Index v_idx = facets(i, j);
                Index uv_idx = uv_indices(i, j);
                REQUIRE(vertices.row(v_idx).isApprox(uv_values.row(uv_idx)));
            }
        }
    }
}
