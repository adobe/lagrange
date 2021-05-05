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

#include <lagrange/combine_mesh_list.h>
#include <lagrange/common.h>
#include <lagrange/create_mesh.h>

TEST_CASE("combine_mesh_list", "[mesh][combine]")
{
    using namespace lagrange;

    SECTION("Simple")
    {
        Vertices3D vertices(4, 3);
        vertices << 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 1.0, 1.0, 0.0;
        Triangles facets(2, 3);
        facets << 0, 1, 2, 2, 1, 3;

        using MeshType = Mesh<Vertices3D, Triangles>;

        auto mesh_shared = to_shared_ptr(wrap_with_mesh(vertices, facets));
        auto mesh_raw = mesh_shared.get();
        std::vector<std::unique_ptr<MeshType>> meshes_unique;
        meshes_unique.push_back(wrap_with_mesh(vertices, facets));
        meshes_unique.push_back(wrap_with_mesh(vertices, facets));
        meshes_unique.push_back(wrap_with_mesh(vertices, facets));

        // All the following should compile and execute with no problem
        auto m1 = combine_mesh_list(meshes_unique);
        auto m2 = combine_mesh_list<decltype(mesh_shared)>({mesh_shared, mesh_shared, mesh_shared});
        auto m3 = combine_mesh_list<decltype(mesh_raw)>({mesh_raw, mesh_raw, mesh_raw});

        using MeshType = typename decltype(m1)::element_type;
        auto verify_mesh = [](MeshType& m) {
            m.initialize_components();
            m.initialize_topology();
            CHECK(m.get_num_components() == 3);
            CHECK(m.is_vertex_manifold());
        };

        verify_mesh(*m1);
        verify_mesh(*m2);
        verify_mesh(*m3);
    }

    SECTION("Attribute preservation")
    {
        Vertices2D vertices(4, 2);
        vertices << 0.0, 0.0, 1.0, 0.0, 0.0, 1.0, 1.0, 1.0;
        Triangles facets(2, 3);
        facets << 0, 1, 2, 2, 1, 3;

        auto mesh = to_shared_ptr(wrap_with_mesh(vertices, facets));
        using MeshTypePtr = decltype(mesh);
        using AttributeArray = MeshTypePtr::element_type::AttributeArray;
        using IndexArray = MeshTypePtr::element_type::IndexArray;
        using UVArray = MeshTypePtr::element_type::UVArray;
        using UVIndices = MeshTypePtr::element_type::UVIndices;

        SECTION("Vertex attribute")
        {
            AttributeArray vertex_indices(4, 1);
            vertex_indices << 0, 1, 2, 3;
            mesh->add_vertex_attribute("index");
            mesh->set_vertex_attribute("index", vertex_indices);
            const auto out_mesh = combine_mesh_list<MeshTypePtr>({mesh, mesh}, true);
            REQUIRE(out_mesh->has_vertex_attribute("index"));

            const auto& indices = out_mesh->get_vertex_attribute("index");
            REQUIRE(indices.rows() == out_mesh->get_num_vertices());
            REQUIRE(indices(0, 0) == indices(4, 0));
            REQUIRE(indices(1, 0) == indices(5, 0));
            REQUIRE(indices(2, 0) == indices(6, 0));
            REQUIRE(indices(3, 0) == indices(7, 0));
        }

        SECTION("Facet attribute")
        {
            AttributeArray facet_indices(2, 1);
            facet_indices << 0, 1;
            mesh->add_facet_attribute("index");
            mesh->set_facet_attribute("index", facet_indices);
            const auto out_mesh = combine_mesh_list<MeshTypePtr>({mesh, mesh}, true);
            REQUIRE(out_mesh->has_facet_attribute("index"));

            const auto& indices = out_mesh->get_facet_attribute("index");
            REQUIRE(indices.rows() == out_mesh->get_num_facets());
            REQUIRE(indices(0, 0) == indices(2, 0));
            REQUIRE(indices(1, 0) == indices(3, 0));
        }

        SECTION("Corner attribute")
        {
            AttributeArray corner_indices(6, 1);
            corner_indices << 0, 1, 2, 3, 4, 5;
            mesh->add_corner_attribute("index");
            mesh->set_corner_attribute("index", corner_indices);
            const auto out_mesh = combine_mesh_list<MeshTypePtr>({mesh, mesh}, true);
            REQUIRE(out_mesh->has_corner_attribute("index"));

            const auto& indices = out_mesh->get_corner_attribute("index");
            REQUIRE(indices.rows() == 12);
            REQUIRE(indices(0, 0) == indices(6, 0));
            REQUIRE(indices(1, 0) == indices(7, 0));
            REQUIRE(indices(2, 0) == indices(8, 0));
            REQUIRE(indices(3, 0) == indices(9, 0));
            REQUIRE(indices(4, 0) == indices(10, 0));
            REQUIRE(indices(5, 0) == indices(11, 0));
        }

#ifdef LA_KEEP_TRANSITION_CODE
        SECTION("Edge attribute")
        {
            const auto num_vertices = mesh->get_num_vertices();
            AttributeArray edge_indices(5, 1);
            edge_indices << 0, 1, 2, 3, 4;
            mesh->initialize_edge_data();
            mesh->add_edge_attribute("index");
            mesh->set_edge_attribute("index", edge_indices);

            const auto out_mesh = combine_mesh_list<MeshTypePtr>({mesh, mesh}, true);
            REQUIRE(out_mesh->is_edge_data_initialized());
            REQUIRE(out_mesh->has_edge_attribute("index"));

            const auto& indices = out_mesh->get_edge_attribute("index");
            for (const auto& e : mesh->get_edges()) {
                const auto ori_id = mesh->get_edge_index(e);
                const auto new_id_1 = out_mesh->get_edge_index(e);
                const auto new_id_2 =
                    out_mesh->get_edge_index({e[0] + num_vertices, e[1] + num_vertices});
                REQUIRE(edge_indices(ori_id, 0) == indices(new_id_1, 0));
                REQUIRE(edge_indices(ori_id, 0) == indices(new_id_2, 0));
            }
        }
#endif

        SECTION("Edge attribute new")
        {
            AttributeArray edge_indices(5, 1);
            edge_indices << 0, 1, 2, 3, 4;
            mesh->initialize_edge_data_new();
            mesh->add_edge_attribute_new("index");
            mesh->set_edge_attribute_new("index", edge_indices);

            const auto out_mesh = combine_mesh_list<MeshTypePtr>({mesh, mesh}, true);
            REQUIRE(out_mesh->is_edge_data_initialized_new());
            REQUIRE(out_mesh->has_edge_attribute_new("index"));

            const auto& indices = out_mesh->get_edge_attribute_new("index");
            REQUIRE(indices.rows() == 10);
            REQUIRE(indices(0, 0) == indices(5, 0));
            REQUIRE(indices(1, 0) == indices(6, 0));
            REQUIRE(indices(2, 0) == indices(7, 0));
            REQUIRE(indices(3, 0) == indices(8, 0));
            REQUIRE(indices(4, 0) == indices(9, 0));
        }

        SECTION("UV")
        {
            UVArray uv(4, 2);
            uv << 0.0, 0.0, 1.0, 0.0, 0.0, 1.0, 1.0, 1.0;
            UVIndices uv_indices(2, 3);
            uv_indices << 0, 1, 2, 2, 1, 3;
            mesh->initialize_uv(uv, uv_indices);

            auto mesh2 = to_shared_ptr(wrap_with_mesh(vertices, facets));

            // Offset uv in X direction;
            uv.col(0).array() += 10;
            mesh2->initialize_uv(uv, uv_indices);

            const auto out_mesh = combine_mesh_list<MeshTypePtr>({mesh, mesh2}, true);
            REQUIRE(out_mesh->is_uv_initialized());
        }

        SECTION("Indexed attribute")
        {
            AttributeArray values(6, 1);
            values << 1, 2, 3, 4, 5, 6;
            IndexArray indices(2, 3);
            indices << 0, 1, 2, 3, 4, 5;
            mesh->add_indexed_attribute("test");
            mesh->set_indexed_attribute("test", values, indices);

            const auto out_mesh = combine_mesh_list<MeshTypePtr>({mesh, mesh}, true);
            REQUIRE(out_mesh->has_indexed_attribute("test"));

            const auto attr = out_mesh->get_indexed_attribute("test");
            const auto out_values = std::get<0>(attr);
            const auto out_indices = std::get<1>(attr);

            REQUIRE(out_values.rows() == 12);
            REQUIRE(out_values.cols() == 1);
            REQUIRE(out_indices.rows() == 4);
            REQUIRE(out_indices.cols() == 3);

            REQUIRE(out_values.minCoeff() == Approx(1));
            REQUIRE(out_values.maxCoeff() == Approx(6));
            REQUIRE(out_indices.minCoeff() == Approx(0));
            REQUIRE(out_indices.maxCoeff() == Approx(11));
        }
    }
}
