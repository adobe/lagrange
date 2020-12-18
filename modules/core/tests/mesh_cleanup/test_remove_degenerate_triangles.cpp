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

#include <algorithm>

#include <lagrange/Mesh.h>
#include <lagrange/common.h>
#include <lagrange/create_mesh.h>
#include <lagrange/mesh_cleanup/detect_degenerate_triangles.h>
#include <lagrange/mesh_cleanup/remove_degenerate_triangles.h>

TEST_CASE("RemoveDegenerateTrianglesTest", "[degenerate][triangle_mesh][cleanup]")
{
    using namespace lagrange;
    Vertices3D vertices(3, 3);
    vertices << 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0, 0.0;
    Triangles facets(1, 3);
    facets << 0, 1, 2;

    auto mesh = create_mesh(vertices, facets);
    REQUIRE(mesh->get_num_vertices() == 3);
    REQUIRE(mesh->get_num_facets() == 1);

    using MeshType = decltype(mesh)::element_type;
    using AttributeArray = typename MeshType::AttributeArray;

    mesh->initialize_uv(vertices.leftCols(2).eval(), facets);
    REQUIRE(mesh->is_uv_initialized());

    using Index = decltype(mesh)::element_type::Index;

    SECTION("No degeneracy")
    {
        auto mesh2 = remove_degenerate_triangles(*mesh);
        REQUIRE(mesh2->get_num_facets() == 1);
        REQUIRE(mesh2->is_uv_initialized());
        REQUIRE(mesh2->get_uv_indices().rows() == 1);
    }

    SECTION("Topological degeneracy")
    {
        facets << 0, 0, 1;
        mesh->import_facets(facets);
        auto mesh2 = remove_degenerate_triangles(*mesh);
        REQUIRE(mesh2->get_num_facets() == 0);
        REQUIRE(mesh2->is_uv_initialized());
        REQUIRE(mesh2->get_uv_indices().rows() == 0);
    }

    SECTION("Geometry degeneracy")
    {
        vertices << 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.5, 0.0, 0.0;
        mesh->import_vertices(vertices);
        auto mesh2 = remove_degenerate_triangles(*mesh);
        REQUIRE(mesh2->get_num_facets() == 0);
        REQUIRE(mesh2->is_uv_initialized());
        REQUIRE(mesh2->get_uv_indices().rows() == 0);
    }

    SECTION("Stacked degeneracy")
    {
        const Index N = 10;
        vertices.resize(N + 1, 3);
        for (Index i = 0; i < N; i++) {
            vertices.row(i) << i, 0.0, 0.0;
        }
        vertices.row(N) << 0, 1, 0;

        facets.resize(N - 1, 3);
        for (Index i = 1; i < N - 1; i++) {
            facets.row(i - 1) << 0, i, N - 1;
        }
        facets.row(N - 2) << 0, N, N - 1; // <-- the only non-degenerate facet.

        // Update UV
        mesh->initialize_uv(vertices.leftCols(2).eval(), facets);
        REQUIRE(mesh->is_uv_initialized());

        mesh->import_vertices(vertices);
        mesh->import_facets(facets);
        REQUIRE(mesh->get_num_vertices() == N + 1);
        REQUIRE(mesh->get_num_facets() == N - 1);

        // Add facet attributes
        AttributeArray indices(N - 1, 1);
        std::iota(indices.data(), indices.data() + N - 1, 0);
        mesh->add_facet_attribute("index");
        mesh->import_facet_attribute("index", indices);

        auto mesh2 = remove_degenerate_triangles(*mesh);
        REQUIRE(mesh2->get_num_facets() == N - 1);
        REQUIRE(mesh2->is_uv_initialized());
        REQUIRE(mesh2->get_uv_indices().rows() == N - 1);
        REQUIRE(mesh2->has_facet_attribute("index"));

        // Check that all output facets are indeed mapped to the single
        // non-degenerate facet in the input.
        auto facet_map = mesh2->get_facet_attribute("index");
        REQUIRE((facet_map.array() == N - 2).all());

        detect_degenerate_triangles(*mesh2);
        const auto& is_degenerate = mesh2->get_facet_attribute("is_degenerate");
        REQUIRE(is_degenerate.rows() == N - 1);
        REQUIRE(is_degenerate.minCoeff() == 0.0);
        REQUIRE(is_degenerate.maxCoeff() == 0.0);

        auto uv_mesh = mesh2->get_uv_mesh();
        detect_degenerate_triangles(*uv_mesh);
        const auto& is_uv_degenerate = uv_mesh->get_facet_attribute("is_degenerate");
        REQUIRE(is_uv_degenerate.rows() == N - 1);
        REQUIRE(is_uv_degenerate.minCoeff() == 0.0);
        REQUIRE(is_uv_degenerate.maxCoeff() == 0.0);
    }

    SECTION("Non-manifold T junction")
    {
        vertices.resize(5, 3);
        vertices << 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 1.0, 1.0, 0.0, 0.5, 0.5, 0.0;
        facets.resize(3, 3);
        facets << 0, 1, 2, 2, 1, 3, 1, 2, 4;

        // SECTION("Per-vertex UV")
        {
            mesh->initialize_uv(vertices.leftCols(2).eval(), facets);
            REQUIRE(mesh->is_uv_initialized());
        }
        // SECTION("Per-corner UV")
        {
            using UVArray = MeshType::UVArray;
            using UVIndices = MeshType::UVIndices;

            UVArray uv(9, 2);
            uv << 0.0, 0.0, 1.0, 0.0, 0.0, 1.0, 0.0, 1.0, 1.0, 0.0, 1.0, 1.0, 1.0, 0.0, 0.0, 1.0,
                0.5, 0.5;

            UVIndices uv_indices(3, 3);
            uv_indices << 0, 1, 2, 3, 4, 5, 6, 7, 8;
            mesh->initialize_uv(uv, uv_indices);
            REQUIRE(mesh->is_uv_initialized());
            REQUIRE(mesh->get_uv_indices().rows() == 3);
            REQUIRE(mesh->get_uv_indices().cols() == 3);
        }

        mesh->import_vertices(vertices);
        mesh->import_facets(facets);
        REQUIRE(mesh->get_num_vertices() == 5);
        REQUIRE(mesh->get_num_facets() == 3);

        auto mesh2 = remove_degenerate_triangles(*mesh);
        REQUIRE(mesh2->get_num_facets() == 4);
        REQUIRE(mesh2->is_uv_initialized());
        REQUIRE(mesh2->get_uv_indices().rows() == 4);

        auto uv_mesh = mesh2->get_uv_mesh();
        detect_degenerate_triangles(*uv_mesh);
        const auto& is_uv_degenerate = uv_mesh->get_facet_attribute("is_degenerate");
        REQUIRE(is_uv_degenerate.rows() == 4);
        REQUIRE(is_uv_degenerate.minCoeff() == 0.0);
        REQUIRE(is_uv_degenerate.maxCoeff() == 0.0);
    }

    SECTION("Degenerate edge")
    {
        vertices.resize(4, 3);
        vertices << 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0, 0.0;

        facets.resize(2, 3);
        facets << 0, 1, 2, 1, 2, 3;

        // Update UV
        mesh->initialize_uv(vertices.leftCols(2).eval(), facets);
        REQUIRE(mesh->is_uv_initialized());

        mesh->import_vertices(vertices);
        mesh->import_facets(facets);
        REQUIRE(mesh->get_num_vertices() == 4);
        REQUIRE(mesh->get_num_facets() == 2);

        auto mesh2 = remove_degenerate_triangles(*mesh);
        REQUIRE(mesh2->get_num_facets() == 1);
        REQUIRE(mesh2->is_uv_initialized());
        REQUIRE(mesh2->get_uv_indices().rows() == 1);

        auto uv_mesh = mesh2->get_uv_mesh();
        detect_degenerate_triangles(*uv_mesh);
        const auto& is_uv_degenerate = uv_mesh->get_facet_attribute("is_degenerate");
        REQUIRE(is_uv_degenerate.rows() == 1);
        REQUIRE(is_uv_degenerate(0, 0) == 0.0);
    }
}
