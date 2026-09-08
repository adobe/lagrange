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
#include <catch2/benchmark/catch_benchmark.hpp>

#include <lagrange/Attribute.h>
#include <lagrange/common.h>
#include <lagrange/compute_edge_lengths.h>
#include <lagrange/mesh_cleanup/remove_short_edges.h>
#include <lagrange/mesh_cleanup/split_long_edges.h>
#include <lagrange/topology.h>
#include <lagrange/views.h>

#ifdef LAGRANGE_ENABLE_LEGACY_FUNCTIONS
    #include <lagrange/Mesh.h>
    #include <lagrange/create_mesh.h>
    #include <lagrange/mesh_convert.h>
#endif

TEST_CASE("remove_short_edges", "[surface][cleanup]")
{
    using namespace lagrange;
    using Scalar = double;
    using Index = uint32_t;
    SurfaceMesh<Scalar, Index> mesh;

    SECTION("empty mesh")
    {
        remove_short_edges(mesh);
        REQUIRE(mesh.get_num_facets() == 0);
        REQUIRE(mesh.get_num_vertices() == 0);
    }

    SECTION("single triangle")
    {
        mesh.add_vertex({0, 0, 0});
        mesh.add_vertex({1, 0, 0});
        mesh.add_vertex({0, 1, 0});
        mesh.add_triangle(0, 1, 2);

        remove_short_edges(mesh);
        REQUIRE(mesh.get_num_facets() == 1);
        REQUIRE(mesh.get_num_vertices() == 3);

        remove_short_edges(mesh, 5.0);
        REQUIRE(mesh.get_num_facets() == 0);
        REQUIRE(mesh.get_num_vertices() == 0);
    }

    SECTION("tet")
    {
        mesh.add_vertex({0, 0, 0});
        mesh.add_vertex({1, 0, 0});
        mesh.add_vertex({0, 1, 0});
        mesh.add_vertex({0, 0, -0.1});
        mesh.add_triangle(0, 1, 2);
        mesh.add_triangle(1, 0, 3);
        mesh.add_triangle(2, 1, 3);
        mesh.add_triangle(0, 2, 3);

        remove_short_edges(mesh, 0.5);
        REQUIRE(mesh.get_num_facets() == 2);
        REQUIRE(mesh.get_num_vertices() == 3);
    }

    SECTION("zero edge")
    {
        mesh.add_vertex({0, 0, 0});
        mesh.add_vertex({1, 0, 0});
        mesh.add_vertex({0, 1, 0});
        mesh.add_vertex({0, 0, 0});
        mesh.add_triangle(0, 1, 2);
        mesh.add_triangle(0, 3, 1);

        remove_short_edges(mesh, 0.5);
        REQUIRE(mesh.get_num_facets() == 1);
        REQUIRE(mesh.get_num_vertices() == 3);
    }

    SECTION("topologically degenerate edge")
    {
        mesh.add_vertex({0, 0, 0});
        mesh.add_vertex({1, 0, 0});
        mesh.add_vertex({0, 1, 0});
        mesh.add_vertex({0, 0, 0});
        mesh.add_triangle(0, 1, 2);
        mesh.add_triangle(0, 3, 1);
        mesh.add_triangle(3, 3, 2);

        remove_short_edges(mesh, 0.5);
        REQUIRE(mesh.get_num_facets() == 1);
        REQUIRE(mesh.get_num_vertices() == 3);
    }

    SECTION("narrow triangle")
    {
        constexpr Scalar tol = 0.01;
        mesh =
            lagrange::testing::load_surface_mesh<Scalar, Index>("open/core/narrow_triangles.obj");
        remove_short_edges(mesh, tol);
        REQUIRE(is_vertex_manifold(mesh));

        auto id = compute_edge_lengths(mesh);
        auto edge_lengths = attribute_vector_view<Scalar>(mesh, id);
        REQUIRE(edge_lengths.minCoeff() > tol);
    }

    SECTION("Stress test")
    {
        // Reducing 10k triangles to 1 triangle.
        constexpr Index N = 10000;
        mesh.add_vertex({1, 0, 0});
        mesh.add_vertex({0, 1, 0});
        for (Index i = 0; i < N; i++) {
            mesh.add_vertex({0, 0, 0});
        }
        for (Index i = 0; i < N; i++) {
            mesh.add_triangle(0, i + 1, i + 2);
        }
        remove_short_edges(mesh);

        REQUIRE(mesh.get_num_facets() == 1);
        REQUIRE(mesh.get_num_vertices() == 3);
    }

    SECTION("Attribute")
    {
        mesh.add_vertex({0, 0, 0});
        mesh.add_vertex({1, 0, 0});
        mesh.add_vertex({0, 1, 0});
        mesh.add_vertex({0, 0, -0.1});
        mesh.add_triangle(0, 1, 2);
        mesh.add_triangle(1, 0, 3);
        mesh.add_triangle(2, 1, 3);
        mesh.add_triangle(0, 2, 3);

        using Index2 = uint64_t;
        Index2 facet_index[4] = {0, 1, 2, 3};
        mesh.create_attribute<Index2>(
            "facet_index",
            AttributeElement::Facet,
            1,
            AttributeUsage::Scalar,
            facet_index);

        remove_short_edges(mesh, 0.5);
        REQUIRE(mesh.has_attribute("facet_index"));
        REQUIRE(mesh.get_num_facets() == 2);
        REQUIRE(mesh.get_num_vertices() == 3);

        auto out_indices = attribute_vector_view<Index2>(mesh, "facet_index");
        REQUIRE(out_indices[0] == 0);
        REQUIRE(out_indices[1] == 2);
    }

    SECTION("mixed elements")
    {
        mesh.add_vertex({0, 0, 0});
        mesh.add_vertex({1, 0, 0});
        mesh.add_vertex({0, 1, 0});
        mesh.add_vertex({1, 1, 0});
        mesh.add_vertex({0, 0, 0.1});
        mesh.add_vertex({1, 0, 0.1});
        mesh.add_vertex({0, 1, 0.1});
        mesh.add_vertex({1, 1, 0.1});
        mesh.add_quad(0, 2, 3, 1);
        mesh.add_quad(4, 5, 7, 6);
        mesh.add_triangle(0, 1, 4);
        mesh.add_triangle(4, 1, 5);

        remove_short_edges(mesh, 0.2);
        REQUIRE(mesh.get_num_facets() == 2);
        REQUIRE(mesh.get_num_vertices() == 6);
    }

    SECTION("custom importance attribute")
    {
        // Use the same tet geometry as the "tet" test, but with custom importance
        mesh.add_vertex({0, 0, 0}); // v0
        mesh.add_vertex({1, 0, 0}); // v1
        mesh.add_vertex({0, 1, 0}); // v2
        mesh.add_vertex({0, 0, -0.1}); // v3 - short edge to v0
        mesh.add_triangle(0, 1, 2);
        mesh.add_triangle(1, 0, 3);
        mesh.add_triangle(2, 1, 3);
        mesh.add_triangle(0, 2, 3);

        // Create custom importance: make v0 low and v3 high
        // Without custom importance, default behavior would keep v0 (interior with higher dihedral)
        // With custom importance, we should keep v3
        auto importance_id = mesh.template create_attribute<Scalar>(
            "my_importance",
            AttributeElement::Vertex,
            AttributeUsage::Scalar,
            1);
        auto importance = mesh.template ref_attribute<Scalar>(importance_id).ref_all();
        importance[0] = 1.0; // v0: low importance
        importance[1] = 50.0; // v1: medium importance
        importance[2] = 50.0; // v2: medium importance
        importance[3] = 100.0; // v3: high importance

        RemoveShortEdgesOptions options;
        options.threshold = 0.5;
        options.vertex_importance_attribute_name = "my_importance";
        remove_short_edges(mesh, options);

        REQUIRE(mesh.get_num_facets() == 2);
        REQUIRE(mesh.get_num_vertices() == 3);

        // Verify the importance attribute still exists
        REQUIRE(mesh.has_attribute("my_importance"));
        auto final_importance = attribute_vector_view<Scalar>(mesh, "my_importance");
        REQUIRE(final_importance.size() == 3);

        // Check that at least one vertex kept high importance (or averaged high)
        Scalar max_importance = final_importance.maxCoeff();
        REQUIRE(max_importance >= 50.0);
    }

    SECTION("options with default importance")
    {
        // Test using options struct without custom importance
        mesh.add_vertex({0, 0, 0});
        mesh.add_vertex({1, 0, 0});
        mesh.add_vertex({0, 1, 0});
        mesh.add_vertex({0, 0, -0.1});
        mesh.add_triangle(0, 1, 2);
        mesh.add_triangle(1, 0, 3);
        mesh.add_triangle(2, 1, 3);
        mesh.add_triangle(0, 2, 3);

        RemoveShortEdgesOptions options;
        options.threshold = 0.5;
        remove_short_edges(mesh, options);

        REQUIRE(mesh.get_num_facets() == 2);
        REQUIRE(mesh.get_num_vertices() == 3);

        // The importance attribute should have been cleaned up
        REQUIRE(!mesh.has_attribute("@vertex_collapse_importance"));
    }

    SECTION("normal flip guard — tight threshold blocks flip-inducing collapse")
    {
        // A short edge (keep_v → remove_v, length = 2*eps) straddles the x-axis.
        //
        //   A(-1,0,0) -------- B(1,0,0)
        //        \    remove_v    /
        //         \   (0,+eps,0) /   <- face (A, B, remove_v): normal +Z
        //          \            /
        //        keep_v(0,-eps,0)
        //
        // After collapsing remove_v onto keep_v, the face (A, B, remove_v) becomes
        // (A, B, keep_v) whose normal points -Z — a full 180° flip.
        // A threshold of pi/4 must reject this collapse.
        // A threshold of pi (disabled) must allow it.
        //
        // We assign high importance to keep_v via a custom attribute so that
        // keep_v is always retained and remove_v is always eliminated.

        constexpr Scalar eps = Scalar(0.004); // edge length = 2*eps = 0.008 < 0.01

        auto make_mesh = [&]() {
            SurfaceMesh<Scalar, Index> m;
            m.add_vertex({-1, 0, 0}); // 0 = A
            m.add_vertex({1, 0, 0}); // 1 = B
            m.add_vertex({0, -eps, 0}); // 2 = keep_v
            m.add_vertex({0, eps, 0}); // 3 = remove_v
            m.add_vertex({0, 0, 1}); // 4 = C (connectivity anchor)

            // The face that flips: (A, B, remove_v) → (A, B, keep_v)
            m.add_triangle(0, 1, 3); // normal +Z before, -Z after

            // Face containing both keep_v and remove_v → becomes degenerate,
            // is excluded from the normal check and cleaned up afterwards.
            m.add_triangle(2, 3, 4);

            // Extra face so keep_v has a 1-ring (prevents it from being isolated).
            m.add_triangle(0, 2, 1); // normal -Z, does not involve remove_v

            // Custom importance: keep_v >> remove_v so the assignment is deterministic.
            auto imp_id = m.template create_attribute<Scalar>(
                "imp",
                AttributeElement::Vertex,
                AttributeUsage::Scalar,
                1);
            auto imp = m.template ref_attribute<Scalar>(imp_id).ref_all();
            imp[0] = 100; // A
            imp[1] = 100; // B
            imp[2] = 200; // keep_v — highest, always retained
            imp[3] = 1; // remove_v — lowest, always eliminated
            imp[4] = 100; // C
            return m;
        };

        // Case 1: tight threshold (45°) — collapse must be blocked.
        {
            auto m = make_mesh();
            RemoveShortEdgesOptions opts;
            opts.threshold = Scalar(0.01);
            opts.max_normal_deviation_angle = lagrange::internal::pi / 4;
            opts.vertex_importance_attribute_name = "imp";
            const Index nf = m.get_num_facets();
            remove_short_edges(m, opts);
            REQUIRE(m.get_num_facets() == nf);
        }

        // Case 2: guard disabled (pi) — collapse must proceed.
        {
            auto m = make_mesh();
            RemoveShortEdgesOptions opts;
            opts.threshold = Scalar(0.01);
            opts.max_normal_deviation_angle = lagrange::internal::pi;
            opts.vertex_importance_attribute_name = "imp";
            const Index nf = m.get_num_facets();
            remove_short_edges(m, opts);
            REQUIRE(m.get_num_facets() < nf);
        }
    }

    SECTION("normal flip guard — safe collapse is still performed")
    {
        // Two triangles that share a nearly-duplicate vertex (v2 ≈ v3).
        // Collapsing the short edge v2-v3 is planar — normals do not change —
        // so it must proceed even with a tight threshold.
        //
        //   v0(0,0,0) ---- v2(1,0,0)
        //       \          / |
        //        \        /  |
        //         \      /   v3(1,0,eps)  <- nearly identical to v2
        //          \    /   /
        //           v1(0,1,0)

        constexpr Scalar eps = Scalar(0.001); // v2-v3 edge length ≈ eps < 0.01

        mesh.add_vertex({0, 0, 0}); // v0
        mesh.add_vertex({0, 1, 0}); // v1
        mesh.add_vertex({1, 0, 0}); // v2
        mesh.add_vertex({1, 0, eps}); // v3 — short edge to v2

        mesh.add_triangle(0, 2, 1); // face using v2
        mesh.add_triangle(1, 2, 3); // face using both v2 and v3

        RemoveShortEdgesOptions opts;
        opts.threshold = Scalar(0.01);
        opts.max_normal_deviation_angle = lagrange::internal::pi / 6; // 30° — tight
        remove_short_edges(mesh, opts);

        // v3 must have been merged into v2: vertex count drops.
        REQUIRE(mesh.get_num_vertices() < 4);
    }
}

TEST_CASE("remove_short_edges benchmark", "[surface][cleanup][!benchmark]")
{
    using namespace lagrange;
    using Scalar = double;
    using Index = uint32_t;

    auto mesh = lagrange::testing::load_surface_mesh<Scalar, Index>("open/core/dragon.obj");

    BENCHMARK("remove_short_edges")
    {
        return remove_short_edges(mesh, 0.02);
    };

#ifdef LAGRANGE_ENABLE_LEGACY_FUNCTIONS
    using MeshType = TriangleMesh3D;
    auto legacy_mesh = to_legacy_mesh<MeshType>(mesh);

    BENCHMARK("legacy::remove_short_edges")
    {
        return remove_short_edges(*legacy_mesh, 0.02);
    };
#endif
}


#ifdef LAGRANGE_ENABLE_LEGACY_FUNCTIONS
TEST_CASE("RemoveShortEdges", "[short_edges][cleanup]")
{
    using namespace lagrange;

    SECTION("Single triangle")
    {
        Vertices3D vertices(3, 3);
        vertices << 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0, 0.0;

        Triangles facets(1, 3);
        facets << 0, 1, 2;

        auto mesh = create_mesh(vertices, facets);
        auto mesh2 = remove_short_edges(*mesh, 0.5);
        auto mesh3 = remove_short_edges(*mesh, 2.0);

        REQUIRE(mesh->get_num_vertices() == mesh2->get_num_vertices());
        REQUIRE(mesh->get_num_facets() == mesh2->get_num_facets());
        REQUIRE((mesh->get_vertices() - mesh2->get_vertices()).norm() == 0.0);

        REQUIRE(mesh3->get_num_vertices() == 0);
        REQUIRE(mesh3->get_num_facets() == 0);
    }

    SECTION("two triangle")
    {
        Vertices3D vertices(4, 3);
        vertices << 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, -0.1;

        Triangles facets(4, 3);
        facets << 0, 1, 2, 1, 0, 3, 2, 1, 3, 0, 2, 3;

        auto mesh = create_mesh(vertices, facets);
        auto mesh2 = remove_short_edges(*mesh, 0.5);

        REQUIRE(3 == mesh2->get_num_vertices());
        REQUIRE(2 == mesh2->get_num_facets()); // Two oppositely oriented facets.
    }

    SECTION("zero edges")
    {
        Vertices3D vertices(4, 3);
        vertices << 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0;

        Triangles facets(2, 3);
        facets << 0, 1, 2, 0, 3, 1;

        auto mesh = create_mesh(vertices, facets);
        auto mesh2 = remove_short_edges(*mesh);

        REQUIRE(3 == mesh2->get_num_vertices());
        REQUIRE(1 == mesh2->get_num_facets());
    }

    SECTION("topologically degeneracy")
    {
        Vertices3D vertices(4, 3);
        vertices << 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0;

        Triangles facets(3, 3);
        facets << 0, 1, 2, 0, 3, 1, 3, 3, 2;

        auto mesh = create_mesh(vertices, facets);
        auto mesh2 = remove_short_edges(*mesh);

        REQUIRE(3 == mesh2->get_num_vertices());
        REQUIRE(1 == mesh2->get_num_facets());
    }

    SECTION("Narrow triangles")
    {
        constexpr double tol = 0.01;
        auto mesh = lagrange::testing::load_mesh<TriangleMesh3D>("open/core/narrow_triangles.obj");
        auto mesh2 = remove_short_edges(*mesh, tol);
        mesh2->initialize_topology();
        REQUIRE(mesh2->is_vertex_manifold());

        compute_edge_lengths(*mesh2);
        const auto& edge_lengths = mesh2->get_edge_attribute("length");
        REQUIRE(edge_lengths.rows() == mesh2->get_num_edges());
        REQUIRE(edge_lengths.minCoeff() > tol);
    }

    SECTION("Densely sampled triangle")
    {
        constexpr double tol = 0.1;
        Vertices3D vertices(3, 3);
        vertices << 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0, 0.0;

        Triangles facets(1, 3);
        facets << 0, 1, 2;

        auto mesh = create_mesh(vertices, facets);
        mesh = split_long_edges(*mesh, 4 * tol * tol, true);
        mesh = remove_short_edges(*mesh, tol);

        compute_edge_lengths(*mesh);
        const auto& edge_lengths = mesh->get_edge_attribute("length");
        REQUIRE(edge_lengths.rows() == mesh->get_num_edges());
        REQUIRE(edge_lengths.minCoeff() > tol);
    }

    SECTION("Stress test")
    {
        // Reducing 10k triangles to 1 triangle.
        constexpr int N = 10000;
        Vertices3D vertices(N + 2, 3);
        vertices.setZero();
        vertices.row(0) << 1.0, 0.0, 0.0;
        vertices.row(1) << 0.0, 1.0, 0.0;
        Triangles facets(N, 3);
        facets.row(0) << 0, 1, 2;
        for (auto i : range(1, N)) {
            facets.row(i) << 1, i + 1, i + 2;
        }
        REQUIRE(facets.maxCoeff() < N + 2);

        auto mesh = create_mesh(vertices, facets);
        mesh = remove_short_edges(*mesh);

        REQUIRE(mesh->get_num_facets() == 1);
        REQUIRE(mesh->get_num_vertices() == 3);
    }

    SECTION("Attributes")
    {
        Vertices3D vertices(4, 3);
        vertices << 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0;

        Triangles facets(3, 3);
        facets << 0, 1, 2, 0, 3, 1, 3, 3, 2;

        auto mesh = create_mesh(vertices, facets);

        using MeshType = decltype(mesh)::element_type;
        using AttributeArray = typename MeshType::AttributeArray;

        AttributeArray facet_indices(3, 1);
        facet_indices << 0, 1, 2;
        mesh->add_facet_attribute("index");
        mesh->set_facet_attribute("index", facet_indices);

        auto mesh2 = remove_short_edges(*mesh);

        REQUIRE(3 == mesh2->get_num_vertices());
        REQUIRE(1 == mesh2->get_num_facets());

        REQUIRE(mesh2->has_facet_attribute("index"));
        const auto& attr = mesh2->get_facet_attribute("index");
        REQUIRE(attr.rows() == 1);
        REQUIRE(attr(0, 0) == 0);
    }
}

TEST_CASE("RemoveShortEdges2", "[short_edges][cleanup]" LA_CORP_FLAG)
{
    using namespace lagrange;
    SECTION("EUC_10594")
    {
        auto mesh = lagrange::testing::load_mesh<TriangleMesh3D>("corp/core/EUC_10594.obj");
        auto mesh2 = remove_short_edges(*mesh);
        compute_edge_lengths(*mesh2);
        const auto& edge_lengths = mesh2->get_edge_attribute("length");
        REQUIRE(edge_lengths.minCoeff() > 0.0);
    }
}
#endif
