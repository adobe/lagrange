/*
 * Copyright 2019 Adobe. All rights reserved.
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
#include <catch2/benchmark/catch_benchmark.hpp>
#include <catch2/catch_approx.hpp>

#include <lagrange/attributes/attribute_utils.h>
#include <lagrange/bvh/zip_boundary.h>
#include <lagrange/combine_mesh_list.h>
#include <lagrange/common.h>
#include <lagrange/compute_euler.h>
#include <lagrange/compute_vertex_normal.h>
#include <lagrange/create_mesh.h>
#include <lagrange/io/load_mesh.impl.h>

TEST_CASE("zip_boundary", "[bvh][zip][boundary]")
{
    using namespace lagrange;
    using MeshType = TriangleMesh3D;
    using VertexArray = MeshType::VertexArray;
    using FacetArray = MeshType::FacetArray;
    using Scalar = MeshType::Scalar;
    using Index = MeshType::Index;

    SECTION("Simple")
    {
        VertexArray vertices(6, 3);
        vertices << 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 1.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 1.0, 0.0, 0.0,
            1.0, 0.0;
        FacetArray facets(2, 3);
        facets << 0, 1, 2, 3, 4, 5;

        auto mesh = create_mesh(vertices, facets);
        REQUIRE(mesh->get_num_vertices() == 6);
        REQUIRE(mesh->get_num_facets() == 2);

        mesh = bvh::zip_boundary(*mesh, 0.1);
        REQUIRE(mesh->get_num_vertices() == 4);
        REQUIRE(mesh->get_num_facets() == 2);

        mesh->initialize_topology();
        REQUIRE(mesh->is_vertex_manifold());
    }

    SECTION("Non-manifold")
    {
        VertexArray vertices(9, 3);
        vertices << 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0,
            2.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 3.0, 0.0;
        FacetArray facets(3, 3);
        facets << 0, 1, 2, 3, 4, 5, 6, 7, 8;

        auto mesh = create_mesh(vertices, facets);
        REQUIRE(mesh->get_num_vertices() == 9);
        REQUIRE(mesh->get_num_facets() == 3);

        SECTION("Small radius")
        {
            mesh = bvh::zip_boundary(*mesh, 0.1);
            REQUIRE(mesh->get_num_vertices() == 5);
            REQUIRE(mesh->get_num_facets() == 3);

            mesh->initialize_topology();
            REQUIRE(!mesh->is_vertex_manifold());
        }

        SECTION("Large radius")
        {
            mesh = bvh::zip_boundary(*mesh, 10.0);
            // All vertices are collapsed into one vertex.
            REQUIRE(mesh->get_num_vertices() == 1);
            // All facets are still there, but they are all degenerate.
            REQUIRE(mesh->get_num_facets() == 3);
        }

        SECTION("Zero radius")
        {
            // The current behavior of Nanoflann is that:
            // With zero radius, no vertices will be collapsed.
            mesh = bvh::zip_boundary(*mesh, 0.0);
            REQUIRE(mesh->get_num_vertices() == 9);
            REQUIRE(mesh->get_num_facets() == 3);
        }
    }

    SECTION("Interior vertices")
    {
        VertexArray vertices(5, 3);
        vertices << 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 1.0, 1.0, 0.0, 0.0, 1.0, 0.0, 0.5, 0.5, 0.0;
        FacetArray facets(4, 3);
        facets << 0, 1, 4, 1, 2, 4, 2, 3, 4, 3, 0, 4;

        auto mesh1 = to_shared_ptr(create_mesh(vertices, facets));

        facets.col(1).swap(facets.col(2));
        auto mesh2 = to_shared_ptr(create_mesh(vertices, facets));

        auto mesh = combine_mesh_list<decltype(mesh1)>({mesh1, mesh2});
        mesh->initialize_topology();
        REQUIRE(compute_euler(*mesh) == 2);
        REQUIRE(mesh->is_vertex_manifold());

        mesh = bvh::zip_boundary(*mesh, 1e-6);
        mesh->initialize_topology();
        REQUIRE(compute_euler(*mesh) == 2);
        REQUIRE(mesh->is_vertex_manifold());
    }

    SECTION("Attribute and UV mapping")
    {
        std::vector<std::unique_ptr<MeshType>> meshes;

        {
            // XY facet
            VertexArray vertices(3, 3);
            vertices << 0, 0, 0, 1, 0, 0, 0, 1, 0;
            FacetArray facets(1, 3);
            facets << 0, 2, 1;
            meshes.push_back(create_mesh(vertices, facets));
        }
        {
            // YZ facet
            VertexArray vertices(3, 3);
            vertices << 0, 0, 0, 0, 1, 0, 0, 0, 1;
            FacetArray facets(1, 3);
            facets << 0, 2, 1;
            meshes.push_back(create_mesh(vertices, facets));
        }
        {
            // ZX facet
            VertexArray vertices(3, 3);
            vertices << 0, 0, 0, 0, 0, 1, 1, 0, 0;
            FacetArray facets(1, 3);
            facets << 0, 2, 1;
            meshes.push_back(create_mesh(vertices, facets));
        }
        {
            // XYZ facet
            VertexArray vertices(3, 3);
            vertices << 1, 0, 0, 0, 1, 0, 0, 0, 1;
            FacetArray facets(1, 3);
            facets << 0, 1, 2;
            meshes.push_back(create_mesh(vertices, facets));
        }

        Scalar x_offset = 0.0;
        for (auto& mesh : meshes) {
            compute_vertex_normal(*mesh);
            map_vertex_attribute_to_corner_attribute(*mesh, "normal");
            REQUIRE(mesh->has_vertex_attribute("normal"));
            REQUIRE(mesh->has_corner_attribute("normal"));

            MeshType::UVArray uv(3, 2);
            uv << 0.0 + x_offset, 0.0, 1.0 + x_offset, 0.0, 0.0 + x_offset, 1.0;
            MeshType::UVIndices uv_indices(1, 3);
            uv_indices << 0, 1, 2;
            mesh->initialize_uv(uv, uv_indices);
            REQUIRE(mesh->is_uv_initialized());
            x_offset += 2;
        }

        auto out_mesh = combine_mesh_list(meshes, true);
        REQUIRE(out_mesh->has_vertex_attribute("normal"));
        REQUIRE(out_mesh->has_corner_attribute("normal"));
        REQUIRE(out_mesh->is_uv_initialized());
        out_mesh->initialize_components();
        REQUIRE(out_mesh->get_num_components() == safe_cast<Index>(meshes.size()));

        out_mesh = bvh::zip_boundary(*out_mesh, 1e-12);
        REQUIRE(out_mesh->has_vertex_attribute("normal"));
        REQUIRE(out_mesh->has_corner_attribute("normal"));
        REQUIRE(out_mesh->is_uv_initialized());
        out_mesh->initialize_components();
        REQUIRE(out_mesh->get_num_components() == 1);

        const auto uv_mesh = out_mesh->get_uv_mesh();
        uv_mesh->initialize_components();
        REQUIRE(uv_mesh->get_num_components() == safe_cast<Index>(meshes.size()));

        const auto& uv = uv_mesh->get_vertices();
        REQUIRE(uv.col(0).minCoeff() == Catch::Approx(0.0));
        REQUIRE(uv.col(0).maxCoeff() == Catch::Approx(7.0));
    }
}

TEST_CASE("zip_boundary_benchmark", "[zip_boundary][!benchmark]")
{
    using namespace lagrange;
    using Scalar = float;
    using Index = uint32_t;
    using MeshType = Mesh<
        Eigen::Matrix<Scalar, Eigen::Dynamic, 3, Eigen::RowMajor>,
        Eigen::Matrix<Index, Eigen::Dynamic, 3, Eigen::RowMajor>>;

    auto closed_mesh = testing::load_mesh<MeshType>("open/core/blub_open_filled.obj");
    auto open_mesh = testing::load_mesh<MeshType>("open/core/blub_open.obj");

    BENCHMARK("closed mesh")
    {
        return bvh::zip_boundary(*closed_mesh, 1e-6f);
    };
    BENCHMARK("open mesh")
    {
        return bvh::zip_boundary(*open_mesh, 1e-6f);
    };
}
