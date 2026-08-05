/*
 * Copyright 2026 Adobe. All rights reserved.
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

#include <lagrange/IndexedAttribute.h>
#include <lagrange/SurfaceMesh.h>
#include <lagrange/internal/split_edges.h>
#include <lagrange/utils/function_ref.h>
#include <lagrange/utils/span.h>
#include <lagrange/views.h>

#include <vector>

TEST_CASE("internal::split_edges_only", "[core]")
{
    using Scalar = double;
    using Index = uint32_t;
    using namespace lagrange;

    // Kept as plain lambdas (not pre-wrapped in function_ref) so they stay alive for the
    // duration of the calls below; function_ref is non-owning and would otherwise dangle.
    auto always_active = [](Index) { return true; };

    SECTION("Single triangle, split one edge into a quad")
    {
        SurfaceMesh<Scalar, Index> mesh;
        mesh.add_vertex({0, 0, 0});
        mesh.add_vertex({1, 0, 0});
        mesh.add_vertex({0, 1, 0});
        mesh.add_triangle(0, 1, 2);

        // Split point in the middle of edge (0, 1).
        Index split_vertex = mesh.get_num_vertices();
        mesh.add_vertex({0.5, 0, 0});

        mesh.initialize_edges();
        Index split_eid = mesh.find_edge_from_vertices(0, 1);
        std::vector<Index> split_pts = {split_vertex};
        auto get_edge_split_pts = [&](Index eid) -> span<Index> {
            if (eid == split_eid) return span<Index>(split_pts.data(), split_pts.size());
            return span<Index>(split_pts.data(), 0);
        };

        auto updated_facets =
            internal::split_edges_only<Scalar, Index>(mesh, get_edge_split_pts, always_active);

        REQUIRE(updated_facets.size() == 1);
        CHECK(updated_facets[0] == 0);

        REQUIRE(mesh.get_num_facets() == 2);
        CHECK(mesh.get_facet_size(0) == 3); // Original triangle is left untouched.

        REQUIRE(mesh.get_facet_size(1) == 4);
        auto quad = mesh.get_facet_vertices(1);
        CHECK(quad[0] == 0);
        CHECK(quad[1] == split_vertex);
        CHECK(quad[2] == 1);
        CHECK(quad[3] == 2);
    }

    SECTION("Triangulated square, split diagonal into two quads")
    {
        SurfaceMesh<Scalar, Index> mesh;
        mesh.add_vertex({0, 0, 0});
        mesh.add_vertex({1, 0, 0});
        mesh.add_vertex({1, 1, 0});
        mesh.add_vertex({0, 1, 0});
        mesh.add_triangle(0, 1, 2);
        mesh.add_triangle(0, 2, 3);

        // Split point in the middle of the diagonal (0, 2).
        Index split_vertex = mesh.get_num_vertices();
        mesh.add_vertex({0.5, 0.5, 0});

        mesh.initialize_edges();
        Index split_eid = mesh.find_edge_from_vertices(0, 2);
        std::vector<Index> split_pts = {split_vertex};
        auto get_edge_split_pts = [&](Index eid) -> span<Index> {
            if (eid == split_eid) return span<Index>(split_pts.data(), split_pts.size());
            return span<Index>(split_pts.data(), 0);
        };

        auto updated_facets =
            internal::split_edges_only<Scalar, Index>(mesh, get_edge_split_pts, always_active);

        REQUIRE(updated_facets.size() == 2);
        CHECK(updated_facets[0] == 0);
        CHECK(updated_facets[1] == 1);

        REQUIRE(mesh.get_num_facets() == 4);
        CHECK(mesh.get_facet_size(0) == 3); // Original triangles are left untouched.
        CHECK(mesh.get_facet_size(1) == 3);

        REQUIRE(mesh.get_facet_size(2) == 4);
        auto quad0 = mesh.get_facet_vertices(2);
        CHECK(quad0[0] == 0);
        CHECK(quad0[1] == 1);
        CHECK(quad0[2] == 2);
        CHECK(quad0[3] == split_vertex);

        REQUIRE(mesh.get_facet_size(3) == 4);
        auto quad1 = mesh.get_facet_vertices(3);
        CHECK(quad1[0] == 0);
        CHECK(quad1[1] == split_vertex);
        CHECK(quad1[2] == 2);
        CHECK(quad1[3] == 3);
    }

    SECTION("Attribute propagation")
    {
        SurfaceMesh<Scalar, Index> mesh;
        mesh.add_vertex({0, 0, 0});
        mesh.add_vertex({1, 0, 0});
        mesh.add_vertex({0, 1, 0});
        mesh.add_triangle(0, 1, 2);

        mesh.create_attribute<Scalar>(
            "facet_value",
            AttributeElement::Facet,
            AttributeUsage::Scalar);
        attribute_vector_ref<Scalar>(mesh, "facet_value") << 42;

        mesh.create_attribute<Scalar>(
            "corner_value",
            AttributeElement::Corner,
            AttributeUsage::Scalar);
        // Corner 0 -> vertex 0, corner 1 -> vertex 1, corner 2 -> vertex 2.
        attribute_vector_ref<Scalar>(mesh, "corner_value") << 10, 20, 40;

        auto indexed_id = mesh.create_attribute<Scalar>(
            "indexed_value",
            AttributeElement::Indexed,
            AttributeUsage::Scalar,
            1);
        {
            auto& iattr = mesh.ref_indexed_attribute<Scalar>(indexed_id);
            iattr.values().resize_elements(3);
            auto values = iattr.values().ref_all();
            values[0] = 10;
            values[1] = 20;
            values[2] = 40;
            auto indices = iattr.indices().ref_all();
            indices[0] = 0;
            indices[1] = 1;
            indices[2] = 2;
        }

        // Split point in the middle of edge (0, 1).
        Index split_vertex = mesh.get_num_vertices();
        mesh.add_vertex({0.5, 0, 0});

        mesh.initialize_edges();
        Index split_eid = mesh.find_edge_from_vertices(0, 1);
        std::vector<Index> split_pts = {split_vertex};
        auto get_edge_split_pts = [&](Index eid) -> span<Index> {
            if (eid == split_eid) return span<Index>(split_pts.data(), split_pts.size());
            return span<Index>(split_pts.data(), 0);
        };

        auto updated_facets =
            internal::split_edges_only<Scalar, Index>(mesh, get_edge_split_pts, always_active);
        REQUIRE(updated_facets.size() == 1);
        REQUIRE(mesh.get_num_facets() == 2);
        REQUIRE(mesh.get_facet_size(1) == 4);

        // New quad facet inherits the original facet's attribute value.
        CHECK(attribute_vector_ref<Scalar>(mesh, "facet_value")[1] == 42);

        // New quad corners are [v0, split_vertex, v1, v2]; split_vertex is the midpoint of
        // edge (v0, v1), so its value interpolates corner_value[0] and corner_value[1].
        auto corner_value = attribute_vector_ref<Scalar>(mesh, "corner_value");
        Index quad_corner_begin = mesh.get_facet_corner_begin(1);
        CHECK(corner_value[quad_corner_begin + 0] == 10);
        CHECK(corner_value[quad_corner_begin + 1] == 15);
        CHECK(corner_value[quad_corner_begin + 2] == 20);
        CHECK(corner_value[quad_corner_begin + 3] == 40);

        // Indexed attribute round-trips through the same interpolation.
        auto& iattr = mesh.get_indexed_attribute<Scalar>("indexed_value");
        auto indexed_values = iattr.values().get_all();
        auto indexed_indices = iattr.indices().get_all();
        CHECK(indexed_values[indexed_indices[quad_corner_begin + 0]] == 10);
        CHECK(indexed_values[indexed_indices[quad_corner_begin + 1]] == 15);
        CHECK(indexed_values[indexed_indices[quad_corner_begin + 2]] == 20);
        CHECK(indexed_values[indexed_indices[quad_corner_begin + 3]] == 40);
    }

    SECTION("Splitting a geometrically degenerate edge throws")
    {
        SurfaceMesh<Scalar, Index> mesh;
        mesh.add_vertex({0, 0, 0});
        mesh.add_vertex({0, 0, 0}); // Coincident with vertex 0, so edge (0, 1) is degenerate.
        mesh.add_vertex({0, 1, 0});
        mesh.add_triangle(0, 1, 2);

        // Split point on the degenerate edge (0, 1): interpolation weight is undefined.
        Index split_vertex = mesh.get_num_vertices();
        mesh.add_vertex({0, 0, 0});

        mesh.initialize_edges();
        Index split_eid = mesh.find_edge_from_vertices(0, 1);
        std::vector<Index> split_pts = {split_vertex};
        auto get_edge_split_pts = [&](Index eid) -> span<Index> {
            if (eid == split_eid) return span<Index>(split_pts.data(), split_pts.size());
            return span<Index>(split_pts.data(), 0);
        };

        LA_REQUIRE_THROWS(
            internal::split_edges_only<Scalar, Index>(mesh, get_edge_split_pts, always_active));
    }
}

TEST_CASE("internal::split_edges shared split vertex", "[core]")
{
    using Scalar = double;
    using Index = uint32_t;
    using namespace lagrange;

    auto always_active = [](Index) { return true; };

    // Two triangles with overlapping collinear edges on the x-axis, both carrying the same split
    // vertex `c`; each triangle must interpolate `c` along its own edge, not a shared parent edge.
    const Index a0 = 0, a1 = 1, a_apex = 2; // triangle A: edge (a0, a1) spans x = 0..4
    const Index b0 = 3, b1 = 4, b_apex = 5; // triangle B: edge (b0, b1) spans x = 1..3
    const Index c = 6; // midpoint of both edges

    auto make_mesh = [&]() {
        SurfaceMesh<Scalar, Index> mesh;
        mesh.add_vertex({0, 0, 0});
        mesh.add_vertex({4, 0, 0});
        mesh.add_vertex({2, 2, 0});
        mesh.add_vertex({1, 0, 0});
        mesh.add_vertex({3, 0, 0});
        mesh.add_vertex({2, -2, 0});
        mesh.add_vertex({2, 0, 0});
        mesh.add_triangle(a0, a1, a_apex);
        mesh.add_triangle(b0, b1, b_apex);
        mesh.create_attribute<Scalar>(
            "corner_value",
            AttributeElement::Corner,
            AttributeUsage::Scalar);
        attribute_vector_ref<Scalar>(mesh, "corner_value") << 10, 40, 100, 0, 60, 200;

        auto indexed_id = mesh.create_attribute<Scalar>(
            "indexed_value",
            AttributeElement::Indexed,
            AttributeUsage::Scalar,
            1);
        {
            auto& iattr = mesh.ref_indexed_attribute<Scalar>(indexed_id);
            iattr.values().resize_elements(6);
            auto values = iattr.values().ref_all();
            values[0] = 10, values[1] = 40, values[2] = 100, values[3] = 0, values[4] = 60,
            values[5] = 200;
            auto indices = iattr.indices().ref_all();
            for (Index i = 0; i < 6; ++i) indices[i] = i;
        }
        return mesh;
    };

    // Run `split_fn` on a fresh mesh with `c` declared as a split point on both collinear edges.
    auto split = [&](auto&& split_fn) {
        auto mesh = make_mesh();
        mesh.initialize_edges();
        const Index ea = mesh.find_edge_from_vertices(a0, a1);
        const Index eb = mesh.find_edge_from_vertices(b0, b1);
        std::vector<Index> split_pts = {c};
        auto get_edge_split_pts = [&](Index eid) -> span<Index> {
            if (eid == ea || eid == eb) return span<Index>(split_pts.data(), split_pts.size());
            return span<Index>(split_pts.data(), 0);
        };
        split_fn(mesh, get_edge_split_pts, always_active);
        return mesh;
    };

    // In triangle A, c is the midpoint of (a0, a1) -> 0.5 * 10 + 0.5 * 40 = 25.
    // In triangle B, c is the midpoint of (b0, b1) -> 0.5 * 0 + 0.5 * 60 = 30.
    auto verify = [&](SurfaceMesh<Scalar, Index>& mesh) {
        const Scalar expected_a = 25;
        const Scalar expected_b = 30;
        auto corner_value = attribute_vector_ref<Scalar>(mesh, "corner_value");
        auto& iattr = mesh.get_indexed_attribute<Scalar>("indexed_value");
        auto indexed_values = iattr.values().get_all();
        auto indexed_indices = iattr.indices().get_all();
        Index checked_a = 0;
        Index checked_b = 0;
        for (Index f = 2; f < mesh.get_num_facets(); ++f) {
            auto fv = mesh.get_facet_vertices(f);
            const Index cb = mesh.get_facet_corner_begin(f);
            bool from_a = false;
            bool from_b = false;
            for (Index k = 0; k < mesh.get_facet_size(f); ++k) {
                if (fv[k] == a0 || fv[k] == a1 || fv[k] == a_apex) from_a = true;
                if (fv[k] == b0 || fv[k] == b1 || fv[k] == b_apex) from_b = true;
            }
            REQUIRE(from_a != from_b);
            for (Index k = 0; k < mesh.get_facet_size(f); ++k) {
                if (fv[k] != c) continue;
                CAPTURE(f, from_a);
                const Scalar expected = from_a ? expected_a : expected_b;
                CHECK(corner_value[cb + k] == expected);
                CHECK(indexed_values[indexed_indices[cb + k]] == expected);
                (from_a ? checked_a : checked_b) += 1;
            }
        }
        REQUIRE(checked_a > 0);
        REQUIRE(checked_b > 0);
    };

    SECTION("split_edges")
    {
        auto mesh = split([](SurfaceMesh<Scalar, Index>& m, auto&& get_pts, auto&& active) {
            internal::split_edges<Scalar, Index>(m, get_pts, active);
        });
        verify(mesh);
    }

    SECTION("split_edges_only")
    {
        auto mesh = split([](SurfaceMesh<Scalar, Index>& m, auto&& get_pts, auto&& active) {
            internal::split_edges_only<Scalar, Index>(m, get_pts, active);
        });
        verify(mesh);
    }
}
