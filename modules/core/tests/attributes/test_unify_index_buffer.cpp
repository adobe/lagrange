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
#include <Eigen/Core>

#include <lagrange/attributes/attribute_utils.h>
#include <lagrange/attributes/condense_indexed_attribute.h>
#include <lagrange/attributes/unify_index_buffer.h>
#include <lagrange/common.h>
#include <lagrange/compute_normal.h>
#include <lagrange/create_mesh.h>
#include <lagrange/mesh_cleanup/split_long_edges.h>

TEST_CASE("UnifyIndexBuffer", "[attribute][unify][indexed]")
{
    using namespace lagrange;

    auto ASSERT_MESH_IS_EQUIVALENT = [](auto& mesh1, auto& mesh2) {
        const auto num_facets = mesh1->get_num_facets();
        const auto vertex_per_facet = mesh1->get_vertex_per_facet();
        REQUIRE(mesh2->get_num_facets() == num_facets);
        REQUIRE(mesh2->get_vertex_per_facet() == vertex_per_facet);
        const auto& attr_names = mesh2->get_vertex_attribute_names();
        for (const auto& name : attr_names) {
            REQUIRE(mesh1->has_indexed_attribute(name));

            const auto attr_1 = mesh1->get_indexed_attribute(name);
            const auto& attr_values_1 = std::get<0>(attr_1);
            const auto& attr_indices_1 = std::get<1>(attr_1);
            const auto& attr_values_2 = mesh2->get_vertex_attribute(name);
            const auto& attr_indices_2 = mesh2->get_facets();

            bool all_same = true;
            for (auto i : range(num_facets)) {
                for (auto j : range(vertex_per_facet)) {
                    all_same &=
                        (attr_values_1.row(attr_indices_1(i, j)) ==
                         attr_values_2.row(attr_indices_2(i, j)));
                }
            }
            REQUIRE(all_same);
        }
    };

    auto ASSERT_MESH_IS_SAME = [](auto& mesh1, auto& mesh2) {
        const auto num_facets = mesh1->get_num_facets();
        const auto vertex_per_facet = mesh1->get_vertex_per_facet();
        REQUIRE(mesh1->get_vertices() == mesh2->get_vertices());
        REQUIRE(mesh1->get_facets() == mesh2->get_facets());
        REQUIRE(mesh2->get_num_facets() == num_facets);
        REQUIRE(mesh2->get_vertex_per_facet() == vertex_per_facet);
        const auto& attr_names = mesh2->get_vertex_attribute_names();
        for (const auto& name : attr_names) {
            const auto& attr_values_1 = mesh1->get_vertex_attribute(name);
            const auto& attr_values_2 = mesh2->get_vertex_attribute(name);
            REQUIRE(attr_values_1 == attr_values_2);
        }
    };

    SECTION("Square")
    {
        Vertices2D vertices(4, 2);
        vertices << 0.0, 0.0, 1.0, 0.0, 0.0, 1.0, 1.0, 1.0;
        Triangles facets(2, 3);
        facets << 0, 1, 2, 2, 1, 3;

        auto mesh = create_mesh(vertices, facets);

        using MeshType = decltype(mesh)::element_type;
        using AttributeArray = MeshType::AttributeArray;
        using IndexArray = MeshType::IndexArray;

        SECTION("No change")
        {
            const std::string attr_name = "test";
            AttributeArray attr = vertices;
            IndexArray indices = facets;
            mesh->add_indexed_attribute(attr_name);
            mesh->set_indexed_attribute(attr_name, attr, indices);

            auto unified_mesh = unify_index_buffer(*mesh, {attr_name});
            REQUIRE(unified_mesh->get_num_vertices() == mesh->get_num_vertices());
            REQUIRE(unified_mesh->get_num_facets() == mesh->get_num_facets());

            unified_mesh->initialize_components();
            REQUIRE(unified_mesh->get_num_components() == 1);

            ASSERT_MESH_IS_EQUIVALENT(mesh, unified_mesh);
        }

        SECTION("With seam")
        {
            const std::string attr_name = "test";
            AttributeArray attr(6, 2);
            attr << 0.0, 0.0, 1.0, 0.0, 0.0, 1.0, 0.0, 1.0, 1.0, 0.0, 1.0, 1.0;
            IndexArray indices(2, 3);
            indices << 0, 1, 2, 3, 4, 5;
            mesh->add_indexed_attribute(attr_name);
            mesh->set_indexed_attribute(attr_name, attr, indices);

            for (int k = 0; k < 10; ++k) {
                auto unified_mesh = unify_index_buffer(*mesh, {attr_name});
                REQUIRE(unified_mesh->get_num_vertices() == 6);
                REQUIRE(unified_mesh->get_num_facets() == mesh->get_num_facets());

                unified_mesh->initialize_components();
                REQUIRE(unified_mesh->get_num_components() == 2);

                ASSERT_MESH_IS_EQUIVALENT(mesh, unified_mesh);
            }
        }
    }

    SECTION("Cube")
    {
        auto mesh = create_cube();
        using MeshType = decltype(mesh)::element_type;
        using Scalar = typename MeshType::Scalar;
        REQUIRE(mesh->is_uv_initialized());

        // Add normal as indexed attribute.
        constexpr Scalar EPS = 1e-3;
        compute_normal(*mesh, M_PI * 0.5 - EPS);
        REQUIRE(mesh->has_indexed_attribute("normal"));

        SECTION("with UV")
        {
            auto unified_mesh = unify_index_buffer(*mesh, {"uv"});
            REQUIRE(unified_mesh->get_num_vertices() == 14);
            REQUIRE(unified_mesh->get_num_facets() == mesh->get_num_facets());

            unified_mesh->initialize_components();
            REQUIRE(unified_mesh->get_num_components() == 1);

            ASSERT_MESH_IS_EQUIVALENT(mesh, unified_mesh);
        }

        SECTION("with UV and normal")
        {
            auto unified_mesh = unify_index_buffer(*mesh, {"uv", "normal"});
            REQUIRE(unified_mesh->get_num_vertices() == 24);
            REQUIRE(unified_mesh->get_num_facets() == mesh->get_num_facets());

            unified_mesh->initialize_components();
            REQUIRE(unified_mesh->get_num_components() == 6);

            ASSERT_MESH_IS_EQUIVALENT(mesh, unified_mesh);
        }
    }

    SECTION("Reproducibility")
    {
        auto mesh = create_cube();
        mesh = lagrange::split_long_edges(*mesh, 0.1, true);
        using MeshType = decltype(mesh)::element_type;
        using Scalar = typename MeshType::Scalar;
        REQUIRE(mesh->is_uv_initialized());

        // Add normal as indexed attribute.
        constexpr Scalar EPS = 1e-3;
        compute_normal(*mesh, M_PI * 0.5 - EPS);
        REQUIRE(mesh->has_indexed_attribute("normal"));

        SECTION("with UV")
        {
            auto unified_mesh = unify_index_buffer(*mesh, {"uv"});
            for (int k = 0; k < 50; ++k) {
                auto unified_mesh2 = unify_index_buffer(*mesh, {"uv"});
                ASSERT_MESH_IS_SAME(unified_mesh, unified_mesh2);
                ASSERT_MESH_IS_EQUIVALENT(mesh, unified_mesh2);
            }
        }

        SECTION("with UV and normal")
        {
            auto unified_mesh = unify_index_buffer(*mesh, {"uv", "normal"});
            for (int k = 0; k < 50; ++k) {
                auto unified_mesh2 = unify_index_buffer(*mesh, {"uv", "normal"});
                ASSERT_MESH_IS_SAME(unified_mesh, unified_mesh2);
                ASSERT_MESH_IS_EQUIVALENT(mesh, unified_mesh2);
            }
        }
    }

    SECTION("Regression")
    {
        // TODO
    }
}
