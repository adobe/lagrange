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
#include <catch2/benchmark/catch_benchmark.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <lagrange/Attribute.h>
#include <lagrange/IndexedAttribute.h>
#include <lagrange/bvh/internal/resolve_tjunctions.h>
#include <lagrange/bvh/resolve_tjunctions.h>
#include <lagrange/combine_meshes.h>
#include <lagrange/compute_area.h>
#include <lagrange/extract_submesh.h>
#include <lagrange/subdivision/midpoint_subdivision.h>
#include <lagrange/testing/common.h>
#include <lagrange/topology.h>
#include <lagrange/utils/invalid.h>
#include <lagrange/views.h>

#include <algorithm>
#include <limits>
#include <numeric>
#include <random>
#include <vector>

namespace {

// Runs the shared correctness suite against `resolve`, a callable with the signature
// void(SurfaceMesh<Scalar, Index>&, bvh::ResolveTJunctionsOptions). Both the public
// (sort-and-sweep) and internal (AABB) variants must produce identical results.
template <typename Resolve>
void run_resolve_tjunctions_tests(Resolve&& resolve)
{
    using namespace lagrange;
    using Scalar = double;
    using Index = uint32_t;
    using Options = bvh::ResolveTJunctionsOptions;

    SECTION("basic t-junction 3d")
    {
        SurfaceMesh<Scalar, Index> mesh;
        mesh.add_vertex({0, 0, 0}); // 0: a
        mesh.add_vertex({2, 0, 0}); // 1: b
        mesh.add_vertex({1, 1, 0}); // 2: top
        mesh.add_vertex({1, 0, 0}); // 3: c (on edge a-b)
        mesh.add_vertex({1, -1, 0}); // 4: bottom
        mesh.add_triangle(0, 1, 2); // spans full edge (a, b)
        mesh.add_triangle(0, 4, 3); // (a, bottom, c)
        mesh.add_triangle(3, 4, 1); // (c, bottom, b)

        const Scalar area_before = compute_mesh_area(mesh);
        resolve(mesh, Options{});

        REQUIRE(mesh.get_num_vertices() == 5);
        REQUIRE(mesh.get_num_facets() == 4);
        REQUIRE_THAT(compute_mesh_area(mesh), Catch::Matchers::WithinRel(area_before, 1e-12));

        mesh.initialize_edges();
        REQUIRE(mesh.find_edge_from_vertices(0, 1) == invalid<Index>()); // (a, b) is gone
        REQUIRE(mesh.find_edge_from_vertices(0, 3) != invalid<Index>()); // (a, c)
        REQUIRE(mesh.find_edge_from_vertices(3, 1) != invalid<Index>()); // (c, b)
    }

    SECTION("basic t-junction 2d")
    {
        SurfaceMesh<Scalar, Index> mesh(2);
        mesh.add_vertex({0, 0});
        mesh.add_vertex({2, 0});
        mesh.add_vertex({1, 1});
        mesh.add_vertex({1, 0});
        mesh.add_vertex({1, -1});
        mesh.add_triangle(0, 1, 2);
        mesh.add_triangle(0, 4, 3);
        mesh.add_triangle(3, 4, 1);

        resolve(mesh, Options{});

        REQUIRE(mesh.get_num_vertices() == 5);
        REQUIRE(mesh.get_num_facets() == 4);
        mesh.initialize_edges();
        REQUIRE(mesh.find_edge_from_vertices(0, 1) == invalid<Index>());
    }

    SECTION("multiple vertices on one edge")
    {
        SurfaceMesh<Scalar, Index> mesh;
        mesh.add_vertex({0, 0, 0}); // 0: a
        mesh.add_vertex({3, 0, 0}); // 1: b
        mesh.add_vertex({1.5, 1, 0}); // 2: top
        mesh.add_vertex({1, 0, 0}); // 3: on edge
        mesh.add_vertex({2, 0, 0}); // 4: on edge
        mesh.add_triangle(0, 1, 2);

        resolve(mesh, Options{});

        REQUIRE(mesh.get_num_facets() == 3); // fan of 3 across the two split points
        // The split facet (a pentagon before triangulation) must be fully triangulated.
        for (Index f = 0; f < mesh.get_num_facets(); ++f) REQUIRE(mesh.get_facet_size(f) == 3);
        mesh.initialize_edges();
        REQUIRE(mesh.find_edge_from_vertices(0, 1) == invalid<Index>());
        REQUIRE(mesh.find_edge_from_vertices(0, 3) != invalid<Index>());
        REQUIRE(mesh.find_edge_from_vertices(3, 4) != invalid<Index>());
        REQUIRE(mesh.find_edge_from_vertices(4, 1) != invalid<Index>());
    }

    SECTION("triangulate_affected option controls affected facets")
    {
        // A quad whose bottom edge (0-1) is spanned by a fan of three triangles meeting at
        // vertices 4 and 5, which lie on that edge: the quad edge is the T-junction to split.
        auto make_mesh = [] {
            SurfaceMesh<Scalar, Index> mesh;
            mesh.add_vertex({0, 0, 0}); // 0
            mesh.add_vertex({3, 0, 0}); // 1
            mesh.add_vertex({3, 1, 0}); // 2
            mesh.add_vertex({0, 1, 0}); // 3
            mesh.add_vertex({1, 0, 0}); // 4: on edge 0-1
            mesh.add_vertex({2, 0, 0}); // 5: on edge 0-1
            mesh.add_vertex({Scalar(1.5), -1, 0}); // 6: apex of the triangle fan
            mesh.add_quad(0, 1, 2, 3);
            mesh.add_triangle(0, 4, 6);
            mesh.add_triangle(4, 5, 6);
            mesh.add_triangle(5, 1, 6);
            return mesh;
        };

        SECTION("triangulate_affected=false keeps the split facet as a polygon")
        {
            auto mesh = make_mesh();
            Options options;
            options.triangulate_affected = false;
            resolve(mesh, options);

            // The quad becomes a hexagon; the three triangles are untouched.
            REQUIRE(mesh.get_num_facets() == 4);
            REQUIRE_FALSE(mesh.is_triangle_mesh());
            Index num_tris = 0;
            Index num_hexagons = 0;
            for (Index f = 0; f < mesh.get_num_facets(); ++f) {
                if (mesh.get_facet_size(f) == 3) ++num_tris;
                if (mesh.get_facet_size(f) == 6) ++num_hexagons;
            }
            REQUIRE(num_tris == 3);
            REQUIRE(num_hexagons == 1);
            mesh.initialize_edges();
            REQUIRE(mesh.find_edge_from_vertices(0, 1) == invalid<Index>());
            REQUIRE(mesh.find_edge_from_vertices(0, 4) != invalid<Index>());
            REQUIRE(mesh.find_edge_from_vertices(4, 5) != invalid<Index>());
            REQUIRE(mesh.find_edge_from_vertices(5, 1) != invalid<Index>());
        }

        SECTION("triangulate_affected=true triangulates the split facet (polygon fallback)")
        {
            // A non-triangle (quad) input falls back to triangulate_polygonal_facets.
            auto mesh = make_mesh();
            Options options;
            options.triangulate_affected = true;
            resolve(mesh, options);

            // The hexagon is triangulated (into 4 triangles) alongside the 3 original triangles.
            for (Index f = 0; f < mesh.get_num_facets(); ++f) REQUIRE(mesh.get_facet_size(f) == 3);
            REQUIRE(mesh.get_num_facets() == 7);
            mesh.initialize_edges();
            REQUIRE(mesh.find_edge_from_vertices(0, 1) == invalid<Index>());
        }
    }

    SECTION("no t-junction is a no-op")
    {
        SurfaceMesh<Scalar, Index> mesh;
        mesh.add_vertex({0, 0, 0});
        mesh.add_vertex({1, 0, 0});
        mesh.add_vertex({1, 1, 0});
        mesh.add_vertex({0, 1, 0});
        mesh.add_triangle(0, 1, 2);
        mesh.add_triangle(0, 2, 3);

        resolve(mesh, Options{});

        REQUIRE(mesh.get_num_vertices() == 4);
        REQUIRE(mesh.get_num_facets() == 2);
    }

    SECTION("boundary_only option")
    {
        // Edge (a, b) is shared by two triangles (interior edge); vertex c lies on it.
        auto make_mesh = [] {
            SurfaceMesh<Scalar, Index> mesh;
            mesh.add_vertex({0, 0, 0}); // 0: a
            mesh.add_vertex({2, 0, 0}); // 1: b
            mesh.add_vertex({1, 1, 0}); // 2: top
            mesh.add_vertex({1, -1, 0}); // 3: bottom
            mesh.add_vertex({1, 0, 0}); // 4: c on edge (a, b)
            mesh.add_triangle(0, 1, 2); // (a, b, top)
            mesh.add_triangle(1, 0, 3); // (b, a, bottom) -> edge (a, b) is interior
            return mesh;
        };

        SECTION("default only checks boundary edges")
        {
            auto mesh = make_mesh();
            resolve(mesh, Options{});
            REQUIRE(mesh.get_num_facets() == 2); // interior edge (a, b) left untouched
        }

        SECTION("boundary_only=false resolves interior edge")
        {
            auto mesh = make_mesh();
            Options options;
            options.boundary_only = false;
            resolve(mesh, options);
            REQUIRE(mesh.get_num_facets() == 4);
            mesh.initialize_edges();
            REQUIRE(mesh.find_edge_from_vertices(0, 1) == invalid<Index>());
        }
    }

    SECTION("tolerance controls detection")
    {
        auto make_mesh = [] {
            SurfaceMesh<Scalar, Index> mesh;
            mesh.add_vertex({0, 0, 0});
            mesh.add_vertex({2, 0, 0});
            mesh.add_vertex({1, 1, 0});
            mesh.add_vertex({1, 1e-3, 0}); // near edge (a, b), offset by 1e-3
            mesh.add_triangle(0, 1, 2);
            return mesh;
        };

        SECTION("default tolerance leaves near-vertex untouched")
        {
            auto mesh = make_mesh();
            resolve(mesh, Options{});
            REQUIRE(mesh.get_num_facets() == 1);
        }

        SECTION("loose tolerance splits")
        {
            auto mesh = make_mesh();
            Options options;
            options.tolerance = 1e-2;
            resolve(mesh, options);
            REQUIRE(mesh.get_num_facets() == 2);
        }
    }

    SECTION("unaffected polygon does not force polygon triangulation")
    {
        auto mesh = testing::load_surface_mesh<Scalar, Index>("open/core/tjunction.fbx");
        const Index q0 = mesh.get_num_vertices();
        mesh.add_vertex({10, 10, 0});
        mesh.add_vertex({11, 10, 0});
        mesh.add_vertex({11, 11, 0});
        mesh.add_vertex({10, 11, 0});
        mesh.add_quad(q0, q0 + 1, q0 + 2, q0 + 3);
        REQUIRE(mesh.get_num_facets() == 9);

        Options options;
        options.boundary_only = false;
        resolve(mesh, options);

        mesh.initialize_edges();
        REQUIRE(mesh.get_num_facets() == 14);
        std::vector<Index> triangles;
        Index num_quads = 0;
        for (Index f = 0; f < mesh.get_num_facets(); ++f) {
            if (mesh.get_facet_size(f) == 3) triangles.push_back(f);
            if (mesh.get_facet_size(f) == 4) ++num_quads;
        }
        REQUIRE(num_quads == 1);

        // Split triangles fan to their opposite corner, avoiding the polygon fallback's slivers.
        const auto vertices = vertex_view(mesh);
        Scalar min_area = std::numeric_limits<Scalar>::max();
        Scalar total_area = 0;
        for (Index f : triangles) {
            const auto fv = mesh.get_facet_vertices(f);
            const Eigen::Matrix<Scalar, 1, 3> e1 = vertices.row(fv[1]) - vertices.row(fv[0]);
            const Eigen::Matrix<Scalar, 1, 3> e2 = vertices.row(fv[2]) - vertices.row(fv[0]);
            const Scalar area = Scalar(0.5) * e1.cross(e2).norm();
            min_area = std::min(min_area, area);
            total_area += area;
        }
        REQUIRE(min_area > Scalar(1e-3) * total_area / static_cast<Scalar>(triangles.size()));
    }

    SECTION("affected polygon does not degrade split triangles")
    {
        auto mesh = testing::load_surface_mesh<Scalar, Index>("open/core/tjunction.fbx");
        const Index num_triangle_vertices = mesh.get_num_vertices();
        const Index q = num_triangle_vertices;
        mesh.add_vertex({10, 10, 0});
        mesh.add_vertex({12, 10, 0});
        mesh.add_vertex({12, 11, 0});
        mesh.add_vertex({10, 11, 0});
        mesh.add_vertex({10.5, 10, 0});
        mesh.add_vertex({11.5, 10, 0});
        mesh.add_quad(q, q + 1, q + 2, q + 3);

        Options options;
        options.boundary_only = false;
        resolve(mesh, options);

        // Both components have split edges; a split quad must not change the triangle strategy.
        REQUIRE(mesh.get_num_facets() == 17);
        const auto vertices = vertex_view(mesh);
        Scalar min_area = std::numeric_limits<Scalar>::max();
        Scalar total_area = 0;
        Index num_triangles = 0;
        for (Index f = 0; f < mesh.get_num_facets(); ++f) {
            const auto fv = mesh.get_facet_vertices(f);
            REQUIRE(fv.size() == 3);
            if (std::any_of(fv.begin(), fv.end(), [num_triangle_vertices](Index v) {
                    return v >= num_triangle_vertices;
                })) {
                continue;
            }
            const Eigen::Matrix<Scalar, 1, 3> e1 = vertices.row(fv[1]) - vertices.row(fv[0]);
            const Eigen::Matrix<Scalar, 1, 3> e2 = vertices.row(fv[2]) - vertices.row(fv[0]);
            const Scalar area = Scalar(0.5) * e1.cross(e2).norm();
            min_area = std::min(min_area, area);
            total_area += area;
            ++num_triangles;
        }
        REQUIRE(num_triangles == 13);
        REQUIRE(min_area > Scalar(1e-3) * total_area / num_triangles);
    }

    SECTION("mixed facets sharing a split edge retain indexed attributes")
    {
        SurfaceMesh<Scalar, Index> mesh;
        mesh.add_vertex({0, 0, 0}); // 0
        mesh.add_vertex({4, 0, 0}); // 1
        mesh.add_vertex({2, 2, 0}); // 2: triangle apex
        mesh.add_vertex({4, -2, 0}); // 3
        mesh.add_vertex({0, -2, 0}); // 4
        mesh.add_vertex({1, 0, 0}); // 5: first split point
        mesh.add_vertex({3, 0, 0}); // 6: second split point
        mesh.add_quad(1, 0, 4, 3); // opposite winding along the shared edge
        mesh.add_triangle(0, 1, 2); // triangle comes after the quad in corner storage

        // The shared edge is a UV seam, including at both interpolated split points.
        std::vector<Scalar> uv_values = {4, 100, 0, 100, 0, 98, 4, 98, 0, 0, 4, 0, 2, 2};
        std::vector<Index> uv_indices = {0, 1, 2, 3, 4, 5, 6};
        mesh.template create_attribute<Scalar>(
            "uv",
            AttributeElement::Indexed,
            AttributeUsage::UV,
            2,
            uv_values,
            uv_indices);

        Options options;
        options.boundary_only = false;
        resolve(mesh, options);

        REQUIRE(mesh.get_num_facets() == 7);
        REQUIRE_THAT(compute_mesh_area(mesh), Catch::Matchers::WithinAbs(12., 1e-12));
        mesh.initialize_edges();
        REQUIRE(mesh.find_edge_from_vertices(0, 1) == invalid<Index>());
        REQUIRE(mesh.find_edge_from_vertices(0, 5) != invalid<Index>());
        REQUIRE(mesh.find_edge_from_vertices(5, 6) != invalid<Index>());
        REQUIRE(mesh.find_edge_from_vertices(6, 1) != invalid<Index>());

        const auto positions = vertex_view(mesh);
        const auto& uv_attr = mesh.template get_indexed_attribute<Scalar>("uv");
        const auto uv = matrix_view(uv_attr.values());
        const auto uv_idx = matrix_view(uv_attr.indices());
        for (Index f = 0; f < mesh.get_num_facets(); ++f) {
            const auto fv = mesh.get_facet_vertices(f);
            REQUIRE(fv.size() == 3);
            const Scalar centroid_y =
                (positions(fv[0], 1) + positions(fv[1], 1) + positions(fv[2], 1)) / 3;
            const Scalar uv_offset = centroid_y < 0 ? 100 : 0;
            for (Index k = 0; k < 3; ++k) {
                const Index uid = uv_idx(mesh.get_facet_corner_begin(f) + k, 0);
                REQUIRE_THAT(uv(uid, 0), Catch::Matchers::WithinAbs(positions(fv[k], 0), 1e-12));
                REQUIRE_THAT(
                    uv(uid, 1),
                    Catch::Matchers::WithinAbs(positions(fv[k], 1) + uv_offset, 1e-12));
            }
        }
    }

    SECTION("indexed attribute is preserved")
    {
        SurfaceMesh<Scalar, Index> mesh;
        mesh.add_vertex({0, 0, 0});
        mesh.add_vertex({2, 0, 0});
        mesh.add_vertex({1, 1, 0});
        mesh.add_vertex({1, 0, 0});
        mesh.add_vertex({1, -1, 0});
        mesh.add_triangle(0, 1, 2);
        mesh.add_triangle(0, 4, 3);
        mesh.add_triangle(3, 4, 1);

        // Indexed UV mirroring each corner's (x, y) position.
        std::vector<Scalar> uv_values = {0, 0, 2, 0, 1, 1, 1, 0, 1, -1};
        std::vector<Index> uv_indices = {0, 1, 2, 0, 4, 3, 3, 4, 1};
        mesh.template create_attribute<Scalar>(
            "uv",
            AttributeElement::Indexed,
            AttributeUsage::UV,
            2,
            uv_values,
            uv_indices);

        resolve(mesh, Options{});

        REQUIRE(mesh.get_num_facets() == 4);
        REQUIRE(mesh.has_attribute("uv"));
        auto& uv_attr = mesh.template get_indexed_attribute<Scalar>("uv");
        auto uv = matrix_view(uv_attr.values());
        // Every UV must equal its corresponding vertex (x, y) since UV mirrors position.
        auto positions = vertex_view(mesh);
        auto facets = facet_view(mesh);
        auto uv_idx = matrix_view(uv_attr.indices());
        for (Index f = 0; f < mesh.get_num_facets(); ++f) {
            for (Index k = 0; k < 3; ++k) {
                const Index vid = facets(f, k);
                const Index cid = f * 3 + k;
                const Index uid = uv_idx(cid, 0);
                REQUIRE_THAT(uv(uid, 0), Catch::Matchers::WithinAbs(positions(vid, 0), 1e-12));
                REQUIRE_THAT(uv(uid, 1), Catch::Matchers::WithinAbs(positions(vid, 1), 1e-12));
            }
        }
    }
}

} // namespace

TEST_CASE("bvh::resolve_tjunctions", "[bvh][cleanup]")
{
    run_resolve_tjunctions_tests(
        [](auto& mesh, auto options) { lagrange::bvh::resolve_tjunctions(mesh, options); });
}

TEST_CASE("bvh::internal::resolve_tjunctions", "[bvh][cleanup]")
{
    run_resolve_tjunctions_tests([](auto& mesh, auto options) {
        lagrange::bvh::internal::resolve_tjunctions(mesh, options);
    });
}

TEST_CASE("bvh::resolve_tjunctions benchmark", "[bvh][cleanup][!benchmark]")
{
    using namespace lagrange;
    using Scalar = double;
    using Index = uint32_t;

    // Build a T-junction-heavy mesh: randomly select a fraction of the dragon's facets, subdivide
    // that subset to insert edge-midpoint vertices, then recombine with the untouched remainder.
    const auto full = testing::load_surface_mesh<Scalar, Index>("open/core/dragon.obj");
    const Index num_facets = full.get_num_facets();

    auto make_input = [&](double fraction) {
        std::vector<Index> shuffled(num_facets);
        std::iota(shuffled.begin(), shuffled.end(), Index(0));
        std::shuffle(shuffled.begin(), shuffled.end(), std::mt19937(42));

        const auto num_selected = static_cast<size_t>(fraction * num_facets);
        std::vector<Index> selected(shuffled.begin(), shuffled.begin() + num_selected);
        std::vector<Index> unselected(shuffled.begin() + num_selected, shuffled.end());

        auto selected_mesh = extract_submesh(full, {selected.data(), selected.size()});
        const auto unselected_mesh = extract_submesh(full, {unselected.data(), unselected.size()});
        selected_mesh = subdivision::midpoint_subdivision(selected_mesh);
        return combine_meshes<Scalar, Index>({&selected_mesh, &unselected_mesh});
    };

    const auto input_01 = make_input(0.01);
    const auto input_10 = make_input(0.10);
    const auto input_50 = make_input(0.50);

    // sort-and-sweep is the public variant; AABB is the internal reference alternative.
    BENCHMARK_ADVANCED("sort-and-sweep (1%)")(Catch::Benchmark::Chronometer meter)
    {
        // resolve_tjunctions mutates in place, so give each measured run its own fresh copy.
        std::vector<SurfaceMesh<Scalar, Index>> meshes(static_cast<size_t>(meter.runs()), input_01);
        meter.measure([&](int i) {
            auto& m = meshes[static_cast<size_t>(i)];
            bvh::resolve_tjunctions(m);
            return m.get_num_facets();
        });
    };

    BENCHMARK_ADVANCED("AABB (1%)")(Catch::Benchmark::Chronometer meter)
    {
        std::vector<SurfaceMesh<Scalar, Index>> meshes(static_cast<size_t>(meter.runs()), input_01);
        meter.measure([&](int i) {
            auto& m = meshes[static_cast<size_t>(i)];
            bvh::internal::resolve_tjunctions(m);
            return m.get_num_facets();
        });
    };

    BENCHMARK_ADVANCED("sort-and-sweep (10%)")(Catch::Benchmark::Chronometer meter)
    {
        std::vector<SurfaceMesh<Scalar, Index>> meshes(static_cast<size_t>(meter.runs()), input_10);
        meter.measure([&](int i) {
            auto& m = meshes[static_cast<size_t>(i)];
            bvh::resolve_tjunctions(m);
            return m.get_num_facets();
        });
    };

    BENCHMARK_ADVANCED("AABB (10%)")(Catch::Benchmark::Chronometer meter)
    {
        std::vector<SurfaceMesh<Scalar, Index>> meshes(static_cast<size_t>(meter.runs()), input_10);
        meter.measure([&](int i) {
            auto& m = meshes[static_cast<size_t>(i)];
            bvh::internal::resolve_tjunctions(m);
            return m.get_num_facets();
        });
    };

    BENCHMARK_ADVANCED("sort-and-sweep (50%)")(Catch::Benchmark::Chronometer meter)
    {
        std::vector<SurfaceMesh<Scalar, Index>> meshes(static_cast<size_t>(meter.runs()), input_50);
        meter.measure([&](int i) {
            auto& m = meshes[static_cast<size_t>(i)];
            bvh::resolve_tjunctions(m);
            return m.get_num_facets();
        });
    };

    BENCHMARK_ADVANCED("AABB (50%)")(Catch::Benchmark::Chronometer meter)
    {
        std::vector<SurfaceMesh<Scalar, Index>> meshes(static_cast<size_t>(meter.runs()), input_50);
        meter.measure([&](int i) {
            auto& m = meshes[static_cast<size_t>(i)];
            bvh::internal::resolve_tjunctions(m);
            return m.get_num_facets();
        });
    };
}
