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
#include <lagrange/bvh/compute_uv_overlap.h>

#include <lagrange/Attribute.h>
#include <lagrange/testing/common.h>
#include <lagrange/triangulate_polygonal_facets.h>
#include <lagrange/views.h>

#include <algorithm>

#include <catch2/benchmark/catch_benchmark.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

namespace {

using namespace lagrange;
using Scalar = float;
using Index = uint32_t;

///
/// Build a triangle mesh with a per-vertex UV attribute.
///
/// @p uv_coords  UV coordinates for each vertex (u, v pairs).
/// @p faces      Triangle connectivity (indices into uv_coords).
///
/// The 3-D vertex positions are set to (u, v, 0) for convenience; they are not
/// used by compute_uv_overlap.
///
SurfaceMesh<Scalar, Index> make_uv_mesh(
    const std::vector<std::array<Scalar, 2>>& uv_coords,
    const std::vector<std::array<Index, 3>>& faces)
{
    SurfaceMesh<Scalar, Index> mesh;
    for (auto& uv : uv_coords) {
        mesh.add_vertex({uv[0], uv[1], Scalar(0)});
    }
    for (auto& f : faces) {
        mesh.add_triangle(f[0], f[1], f[2]);
    }

    // Create a per-vertex UV attribute so that uv_mesh_view can extract UV positions.
    const AttributeId uv_id = mesh.template create_attribute<Scalar>(
        "@uv",
        AttributeElement::Vertex,
        AttributeUsage::UV,
        2);
    auto& attr = mesh.template ref_attribute<Scalar>(uv_id);
    auto values = attr.ref_all();
    for (Index i = 0; i < static_cast<Index>(uv_coords.size()); ++i) {
        values[i * 2 + 0] = uv_coords[i][0];
        values[i * 2 + 1] = uv_coords[i][1];
    }
    return mesh;
}

///
/// Run compute_uv_overlap with all three methods, requesting overlapping pairs.
/// Verify that the sorted pair lists are identical across methods.
/// Returns the SweepAndPrune result.
///
/// Must be called from within a Catch2 TEST_CASE (uses REQUIRE/INFO macros).
///
bvh::UVOverlapResult<Scalar, Index> run_all_methods_and_check_pairs(
    SurfaceMesh<Scalar, Index>& mesh)
{
    bvh::UVOverlapOptions opts;
    opts.compute_overlapping_pairs = true;

    opts.method = bvh::UVOverlapMethod::SweepAndPrune;
    auto result_ze = bvh::compute_uv_overlap(mesh, opts);

    opts.method = bvh::UVOverlapMethod::BVH;
    auto result_bvh = bvh::compute_uv_overlap(mesh, opts);

    opts.method = bvh::UVOverlapMethod::Hybrid;
    auto result_h = bvh::compute_uv_overlap(mesh, opts);

    INFO("SweepAndPrune pairs: " << result_ze.overlapping_pairs.size());
    INFO("BVH pairs:           " << result_bvh.overlapping_pairs.size());
    INFO("Hybrid pairs:        " << result_h.overlapping_pairs.size());

    REQUIRE(result_bvh.overlapping_pairs == result_ze.overlapping_pairs);
    REQUIRE(result_h.overlapping_pairs == result_ze.overlapping_pairs);

    return result_ze;
}

} // namespace

// ---------------------------------------------------------------------------
// Basic overlap detection
// ---------------------------------------------------------------------------

TEST_CASE("compute_uv_overlap: no overlap", "[bvh][uv_overlap]")
{
    using namespace lagrange;

    // Two unit right triangles placed far apart in UV space.
    // A: (0,0)-(1,0)-(0,1)    B: (2,0)-(3,0)-(2,1)
    auto mesh =
        make_uv_mesh({{0, 0}, {1, 0}, {0, 1}, {2, 0}, {3, 0}, {2, 1}}, {{{0, 1, 2}}, {{3, 4, 5}}});

    auto result = bvh::compute_uv_overlap(mesh);
    REQUIRE_FALSE(result.overlap_area.has_value());
    REQUIRE(result.overlap_coloring_id == invalid_attribute_id());
}

TEST_CASE("compute_uv_overlap: full overlap (identical triangles)", "[bvh][uv_overlap]")
{
    using namespace lagrange;

    // Two triangles with exactly the same UV coordinates.
    // Area of each = 0.5; intersection = the whole triangle, area = 0.5.
    auto mesh =
        make_uv_mesh({{0, 0}, {1, 0}, {0, 1}, {0, 0}, {1, 0}, {0, 1}}, {{{0, 1, 2}}, {{3, 4, 5}}});

    auto result = bvh::compute_uv_overlap(mesh);
    REQUIRE(result.overlap_area.has_value());
    REQUIRE_THAT(*result.overlap_area, Catch::Matchers::WithinAbs(0.5f, 1e-5f));
}

TEST_CASE("compute_uv_overlap: partial overlap (known area)", "[bvh][uv_overlap]")
{
    using namespace lagrange;

    // A: (0,0)-(1,0)-(0,1)
    // B: (0.5,0)-(1.5,0)-(0.5,1)
    //
    // Sutherland-Hodgman gives intersection polygon: (0.5,0)-(1,0)-(0.5,0.5)
    // Area = 0.5 * 0.5 * 0.5 = 0.125
    auto mesh = make_uv_mesh(
        {{0, 0}, {1, 0}, {0, 1}, {0.5f, 0}, {1.5f, 0}, {0.5f, 1}},
        {{{0, 1, 2}}, {{3, 4, 5}}});

    auto result = bvh::compute_uv_overlap(mesh);
    REQUIRE(result.overlap_area.has_value());
    REQUIRE_THAT(*result.overlap_area, Catch::Matchers::WithinAbs(0.125f, 1e-5f));
}

TEST_CASE("compute_uv_overlap: CW-oriented triangle", "[bvh][uv_overlap]")
{
    using namespace lagrange;

    // Triangle A CCW: (0,0)-(1,0)-(0,1)
    // Triangle B CW (same triangle, reversed): (0,0)-(0,1)-(1,0)
    // These are the same triangle; intersection area = 0.5.
    auto mesh =
        make_uv_mesh({{0, 0}, {1, 0}, {0, 1}, {0, 0}, {0, 1}, {1, 0}}, {{{0, 1, 2}}, {{3, 4, 5}}});

    auto result = bvh::compute_uv_overlap(mesh);
    REQUIRE(result.overlap_area.has_value());
    REQUIRE_THAT(*result.overlap_area, Catch::Matchers::WithinAbs(0.5f, 1e-5f));
}

// ---------------------------------------------------------------------------
// Adjacent triangles must NOT be counted as overlapping
// ---------------------------------------------------------------------------

TEST_CASE("compute_uv_overlap: adjacent triangles sharing an edge", "[bvh][uv_overlap]")
{
    using namespace lagrange;

    // Standard quad split into two triangles sharing edge (1,0)-(0,1).
    //   v0=(0,0)  v1=(1,0)  v2=(0,1)  v3=(1,1)
    //   T0: v0-v1-v2   T1: v1-v3-v2
    auto mesh = make_uv_mesh({{0, 0}, {1, 0}, {0, 1}, {1, 1}}, {{{0, 1, 2}}, {{1, 3, 2}}});

    auto result = bvh::compute_uv_overlap(mesh);
    REQUIRE_FALSE(result.overlap_area.has_value());
}

TEST_CASE("compute_uv_overlap: adjacent triangles sharing a vertex", "[bvh][uv_overlap]")
{
    using namespace lagrange;

    // T0: (0,0)-(1,0)-(0,1)    T1: (1,0)-(2,0)-(1,1)
    // They share vertex (1,0) only; no interior overlap.
    auto mesh =
        make_uv_mesh({{0, 0}, {1, 0}, {0, 1}, {1, 0}, {2, 0}, {1, 1}}, {{{0, 1, 2}}, {{3, 4, 5}}});

    auto result = bvh::compute_uv_overlap(mesh);
    REQUIRE_FALSE(result.overlap_area.has_value());
}

// ---------------------------------------------------------------------------
// Edge cases
// ---------------------------------------------------------------------------

TEST_CASE("compute_uv_overlap: empty mesh", "[bvh][uv_overlap]")
{
    using namespace lagrange;

    SurfaceMesh<Scalar, Index> mesh;
    // Create a UV attribute even on the empty mesh so uv_mesh_view can find it.
    mesh.template create_attribute<Scalar>("@uv", AttributeElement::Vertex, AttributeUsage::UV, 2);

    auto result = bvh::compute_uv_overlap(mesh);
    REQUIRE_FALSE(result.overlap_area.has_value());
    REQUIRE(result.overlap_coloring_id == invalid_attribute_id());
}

TEST_CASE("compute_uv_overlap: single triangle", "[bvh][uv_overlap]")
{
    using namespace lagrange;

    auto mesh = make_uv_mesh({{0, 0}, {1, 0}, {0, 1}}, {{{0, 1, 2}}});

    auto result = bvh::compute_uv_overlap(mesh);
    REQUIRE_FALSE(result.overlap_area.has_value());
}

TEST_CASE("compute_uv_overlap: coloring flag false no attribute created", "[bvh][uv_overlap]")
{
    using namespace lagrange;

    auto mesh =
        make_uv_mesh({{0, 0}, {1, 0}, {0, 1}, {0, 0}, {1, 0}, {0, 1}}, {{{0, 1, 2}}, {{3, 4, 5}}});

    bvh::UVOverlapOptions opts;
    opts.compute_overlap_coloring = false;
    auto result = bvh::compute_uv_overlap(mesh, opts);

    REQUIRE(result.overlap_area.has_value());
    REQUIRE(result.overlap_coloring_id == invalid_attribute_id());
    REQUIRE_FALSE(mesh.has_attribute("@uv_overlap_color"));
}

TEST_CASE("compute_uv_overlap: custom UV attribute name", "[bvh][uv_overlap]")
{
    using namespace lagrange;

    SurfaceMesh<Scalar, Index> mesh;
    mesh.add_vertex({0, 0, 0});
    mesh.add_vertex({1, 0, 0});
    mesh.add_vertex({0, 1, 0});
    mesh.add_vertex({0, 0, 0});
    mesh.add_vertex({1, 0, 0});
    mesh.add_vertex({0, 1, 0});
    mesh.add_triangle(0, 1, 2);
    mesh.add_triangle(3, 4, 5);

    // Create a UV attribute with a custom name.
    const AttributeId uv_id = mesh.template create_attribute<Scalar>(
        "my_uvs",
        AttributeElement::Vertex,
        AttributeUsage::UV,
        2);
    {
        auto& attr = mesh.template ref_attribute<Scalar>(uv_id);
        auto v = attr.ref_all();
        // Both triangles identical in UV → full overlap.
        float coords[] = {0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1};
        std::copy(std::begin(coords), std::end(coords), v.begin());
    }

    bvh::UVOverlapOptions opts;
    opts.uv_attribute_name = "my_uvs";
    auto result = bvh::compute_uv_overlap(mesh, opts);

    REQUIRE(result.overlap_area.has_value());
    REQUIRE_THAT(*result.overlap_area, Catch::Matchers::WithinAbs(0.5f, 1e-5f));
}

// ---------------------------------------------------------------------------
// Per-facet overlap coloring
// ---------------------------------------------------------------------------

TEST_CASE("compute_uv_overlap: coloring chain A-B-C, A does not overlap C", "[bvh][uv_overlap]")
{
    using namespace lagrange;

    // A: (0,0)-(1,0)-(0,1)
    // B: (0.5,0)-(1.5,0)-(0.5,1)   overlaps A
    // C: (1.0,0)-(2.0,0)-(1.0,1)   overlaps B; shares only vertex (1,0) with A → no overlap
    //
    // Overlap graph: A-B, B-C   (A and C are not adjacent)
    // Greedy coloring: A→1, B→2, C→1
    auto mesh = make_uv_mesh(
        {{0, 0}, {1, 0}, {0, 1}, {0.5f, 0}, {1.5f, 0}, {0.5f, 1}, {1.0f, 0}, {2.0f, 0}, {1.0f, 1}},
        {{{0, 1, 2}}, {{3, 4, 5}}, {{6, 7, 8}}});

    bvh::UVOverlapOptions opts;
    opts.compute_overlap_coloring = true;
    auto result = bvh::compute_uv_overlap(mesh, opts);

    REQUIRE(result.overlap_area.has_value());
    REQUIRE(result.overlap_coloring_id != invalid_attribute_id());

    auto colors = attribute_vector_view<Index>(mesh, result.overlap_coloring_id);
    REQUIRE(colors.size() == 3);

    const Index color_a = colors[0];
    const Index color_b = colors[1];
    const Index color_c = colors[2];

    // All overlapping triangles must have a non-zero color.
    REQUIRE(color_a > 0);
    REQUIRE(color_b > 0);
    REQUIRE(color_c > 0);

    // Adjacent (overlapping) triangles must have different colors.
    REQUIRE(color_a != color_b); // A overlaps B
    REQUIRE(color_b != color_c); // B overlaps C

    // Non-adjacent triangles (A and C) may share a color.
    REQUIRE(color_a == color_c);
}

TEST_CASE("compute_uv_overlap: coloring three mutually overlapping triangles", "[bvh][uv_overlap]")
{
    using namespace lagrange;

    // Three triangles all nearly identical → mutually overlapping → need 3 distinct colors.
    // A: (0,0)-(1,0)-(0,1)
    // B: (0.1,0)-(1.1,0)-(0.1,1)
    // C: (0.2,0)-(1.2,0)-(0.2,1)
    auto mesh = make_uv_mesh(
        {{0, 0}, {1, 0}, {0, 1}, {0.1f, 0}, {1.1f, 0}, {0.1f, 1}, {0.2f, 0}, {1.2f, 0}, {0.2f, 1}},
        {{{0, 1, 2}}, {{3, 4, 5}}, {{6, 7, 8}}});

    bvh::UVOverlapOptions opts;
    opts.compute_overlap_coloring = true;
    auto result = bvh::compute_uv_overlap(mesh, opts);

    REQUIRE(result.overlap_area.has_value());
    REQUIRE(result.overlap_coloring_id != invalid_attribute_id());

    auto colors = attribute_vector_view<Index>(mesh, result.overlap_coloring_id);
    REQUIRE(colors.size() == 3);

    // All three must be overlapping → non-zero colors.
    REQUIRE(colors[0] > 0);
    REQUIRE(colors[1] > 0);
    REQUIRE(colors[2] > 0);

    // No two may share a color since all three pairwise-overlap.
    REQUIRE(colors[0] != colors[1]);
    REQUIRE(colors[1] != colors[2]);
    REQUIRE(colors[0] != colors[2]);
}

TEST_CASE("compute_uv_overlap: coloring custom attribute name is used", "[bvh][uv_overlap]")
{
    using namespace lagrange;

    auto mesh =
        make_uv_mesh({{0, 0}, {1, 0}, {0, 1}, {0, 0}, {1, 0}, {0, 1}}, {{{0, 1, 2}}, {{3, 4, 5}}});

    bvh::UVOverlapOptions opts;
    opts.compute_overlap_coloring = true;
    opts.overlap_coloring_attribute_name = "my_colors";
    auto result = bvh::compute_uv_overlap(mesh, opts);

    REQUIRE(result.overlap_area.has_value());
    REQUIRE(mesh.has_attribute("my_colors"));
    REQUIRE_FALSE(mesh.has_attribute("@uv_overlap_color"));
}

TEST_CASE("compute_uv_overlap: coloring and pairs requested together", "[bvh][uv_overlap]")
{
    using namespace lagrange;

    // A overlaps B, B overlaps C, A does not overlap C.
    auto mesh = make_uv_mesh(
        {{0, 0}, {1, 0}, {0, 1}, {0.5f, 0}, {1.5f, 0}, {0.5f, 1}, {1.0f, 0}, {2.0f, 0}, {1.0f, 1}},
        {{{0, 1, 2}}, {{3, 4, 5}}, {{6, 7, 8}}});

    bvh::UVOverlapOptions opts;
    opts.compute_overlap_coloring = true;
    opts.compute_overlapping_pairs = true;
    auto result = bvh::compute_uv_overlap(mesh, opts);

    // Coloring must be valid.
    REQUIRE(result.overlap_coloring_id != invalid_attribute_id());
    auto colors = attribute_vector_view<Index>(mesh, result.overlap_coloring_id);
    REQUIRE(colors[0] != colors[1]);
    REQUIRE(colors[1] != colors[2]);

    // Pairs must also be populated (not destroyed by the coloring phase).
    REQUIRE(result.overlapping_pairs.size() == 2);
    REQUIRE(result.overlapping_pairs[0] == std::make_pair(Index(0), Index(1)));
    REQUIRE(result.overlapping_pairs[1] == std::make_pair(Index(1), Index(2)));
}

// ---------------------------------------------------------------------------
// Method cross-validation: all three methods must produce identical pair lists
// ---------------------------------------------------------------------------

TEST_CASE("compute_uv_overlap: all methods agree on partial overlap pairs", "[bvh][uv_overlap]")
{
    auto mesh = make_uv_mesh(
        {{0, 0}, {1, 0}, {0, 1}, {0.5f, 0}, {1.5f, 0}, {0.5f, 1}},
        {{{0, 1, 2}}, {{3, 4, 5}}});

    auto result = run_all_methods_and_check_pairs(mesh);
    REQUIRE(result.overlap_area.has_value());
    REQUIRE(result.overlapping_pairs.size() == 1);
    REQUIRE(result.overlapping_pairs[0] == std::make_pair(Index(0), Index(1)));
}

TEST_CASE("compute_uv_overlap: all methods agree on no-overlap mesh", "[bvh][uv_overlap]")
{
    auto mesh =
        make_uv_mesh({{0, 0}, {1, 0}, {0, 1}, {2, 0}, {3, 0}, {2, 1}}, {{{0, 1, 2}}, {{3, 4, 5}}});

    auto result = run_all_methods_and_check_pairs(mesh);
    REQUIRE_FALSE(result.overlap_area.has_value());
    REQUIRE(result.overlapping_pairs.empty());
}

TEST_CASE("compute_uv_overlap: all methods agree on adjacent-only mesh", "[bvh][uv_overlap]")
{
    auto mesh = make_uv_mesh({{0, 0}, {1, 0}, {0, 1}, {1, 1}}, {{{0, 1, 2}}, {{1, 3, 2}}});

    auto result = run_all_methods_and_check_pairs(mesh);
    REQUIRE_FALSE(result.overlap_area.has_value());
    REQUIRE(result.overlapping_pairs.empty());
}

TEST_CASE("compute_uv_overlap: all methods agree on chain A-B-C", "[bvh][uv_overlap]")
{
    // A overlaps B, B overlaps C, A does not overlap C.
    auto mesh = make_uv_mesh(
        {{0, 0}, {1, 0}, {0, 1}, {0.5f, 0}, {1.5f, 0}, {0.5f, 1}, {1.0f, 0}, {2.0f, 0}, {1.0f, 1}},
        {{{0, 1, 2}}, {{3, 4, 5}}, {{6, 7, 8}}});

    auto result = run_all_methods_and_check_pairs(mesh);
    REQUIRE(result.overlap_area.has_value());
    REQUIRE(result.overlapping_pairs.size() == 2);
    REQUIRE(result.overlapping_pairs[0] == std::make_pair(Index(0), Index(1)));
    REQUIRE(result.overlapping_pairs[1] == std::make_pair(Index(1), Index(2)));
}

TEST_CASE("compute_uv_overlap: all methods agree on dense overlapping grid", "[bvh][uv_overlap]")
{
    using namespace lagrange;

    // 10x10 grid of unit right-triangles spaced at 0.9 (< step=1.0) so adjacent
    // columns overlap by 0.1 in x, guaranteeing many overlapping pairs.
    std::vector<std::array<Scalar, 2>> uvs;
    std::vector<std::array<Index, 3>> faces;
    const Index n = 10;
    const Scalar step = 1.0f;
    const Scalar spacing = 0.9f; // < step → adjacent triangles overlap
    Index vi = 0;
    for (Index row = 0; row < n; ++row) {
        for (Index col = 0; col < n; ++col) {
            const Scalar x = static_cast<Scalar>(col) * spacing;
            const Scalar y = static_cast<Scalar>(row) * spacing;
            uvs.push_back({x, y});
            uvs.push_back({x + step, y});
            uvs.push_back({x, y + step});
            faces.push_back({vi, vi + 1, vi + 2});
            vi += 3;
        }
    }
    auto mesh = make_uv_mesh(uvs, faces);

    auto result = run_all_methods_and_check_pairs(mesh);
    REQUIRE(result.overlap_area.has_value());
    REQUIRE(result.overlapping_pairs.size() > 0);
}

// ---------------------------------------------------------------------------
// Method cross-validation on real meshes
// ---------------------------------------------------------------------------

TEST_CASE(
    "compute_uv_overlap: all methods agree on Grenade_H",
    "[bvh][uv_overlap][corp]" LA_CORP_FLAG LA_SLOW_DEBUG_FLAG)
{
    using namespace lagrange;

    auto mesh = lagrange::testing::load_surface_mesh<Scalar, Index>("corp/io/Grenade_H.obj");
    lagrange::triangulate_polygonal_facets(mesh);
    REQUIRE(mesh.is_triangle_mesh());

    auto result = run_all_methods_and_check_pairs(mesh);
    REQUIRE(result.overlap_area.has_value());
    REQUIRE(result.overlapping_pairs.size() > 0);
}

// ---------------------------------------------------------------------------
// Benchmarks (disabled by default; run with [!benchmark] tag)
// ---------------------------------------------------------------------------

namespace {

///
/// Build a mesh with @p n unit right triangles tiled in UV space with no overlap.
/// Every other triangle is then shifted to create a controllable overlap fraction.
///
SurfaceMesh<Scalar, Index> make_benchmark_mesh(Index n_per_side, float overlap_shift)
{
    std::vector<std::array<Scalar, 2>> uvs;
    std::vector<std::array<Index, 3>> faces;

    const Scalar step = 1.0f;
    Index vi = 0;
    for (Index row = 0; row < n_per_side; ++row) {
        for (Index col = 0; col < n_per_side; ++col) {
            const Scalar x = static_cast<Scalar>(col) * step * 1.1f;
            const Scalar y = static_cast<Scalar>(row) * step * 1.1f;
            // Apply a small overlap shift to odd triangles.
            const Scalar dx = ((row + col) % 2 == 1) ? overlap_shift : 0.0f;
            uvs.push_back({x + dx, y});
            uvs.push_back({x + dx + step, y});
            uvs.push_back({x + dx, y + step});
            faces.push_back({vi, vi + 1, vi + 2});
            vi += 3;
        }
    }
    return make_uv_mesh(uvs, faces);
}

} // namespace

TEST_CASE("compute_uv_overlap benchmark: synthetic grid", "[bvh][!benchmark]")
{
    using namespace lagrange;

    // 20×20 = 400 triangles, with a small shift so ~half overlap their neighbour.
    auto mesh = make_benchmark_mesh(20, 0.05f);

    BENCHMARK("hybrid (ZE)")
    {
        bvh::UVOverlapOptions opts;
        opts.method = bvh::UVOverlapMethod::Hybrid;
        return bvh::compute_uv_overlap(mesh, opts);
    };

    BENCHMARK("sweep-and-prune (ZE)")
    {
        bvh::UVOverlapOptions opts;
        opts.method = bvh::UVOverlapMethod::SweepAndPrune;
        return bvh::compute_uv_overlap(mesh, opts);
    };

    BENCHMARK("BVH per-triangle")
    {
        bvh::UVOverlapOptions opts;
        opts.method = bvh::UVOverlapMethod::BVH;
        return bvh::compute_uv_overlap(mesh, opts);
    };
}

TEST_CASE("compute_uv_overlap benchmark: dragon", "[bvh][!benchmark]")
{
    using namespace lagrange;

    // Load the dragon mesh and assign UV coordinates via an orthographic XY projection.
    // This gives every vertex u = x and v = y (using the mesh's existing spatial scale),
    // which causes front-facing and back-facing triangles to overlap in UV space —
    // a realistic stress-test that exercises the full overlap pipeline on a
    // complex high-resolution mesh (~870 K triangles).
    auto mesh = lagrange::testing::load_surface_mesh<Scalar, Index>("open/core/dragon.obj");
    la_runtime_assert(mesh.is_triangle_mesh(), "dragon.obj must be triangulated");

    {
        const auto verts = vertex_view(mesh);
        const Index nv = mesh.get_num_vertices();

        // Compute XY bounding box so we can report it, but do not normalise —
        // keeping the original scale preserves the geometric relationship between
        // triangles and avoids artificially concentrating all UVs into [0,1]^2.
        Scalar x_min = std::numeric_limits<Scalar>::max();
        Scalar x_max = std::numeric_limits<Scalar>::lowest();
        Scalar y_min = std::numeric_limits<Scalar>::max();
        Scalar y_max = std::numeric_limits<Scalar>::lowest();
        for (Index i = 0; i < nv; ++i) {
            x_min = std::min(x_min, verts(i, 0));
            x_max = std::max(x_max, verts(i, 0));
            y_min = std::min(y_min, verts(i, 1));
            y_max = std::max(y_max, verts(i, 1));
        }
        INFO(
            "Dragon XY UV extent: [" << x_min << ", " << x_max << "] x [" << y_min << ", " << y_max
                                     << "]");

        const AttributeId uv_id = mesh.template create_attribute<Scalar>(
            "@uv",
            AttributeElement::Vertex,
            AttributeUsage::UV,
            2);
        auto& attr = mesh.template ref_attribute<Scalar>(uv_id);
        auto values = attr.ref_all();
        for (Index i = 0; i < nv; ++i) {
            values[i * 2 + 0] = verts(i, 0); // u = x
            values[i * 2 + 1] = verts(i, 1); // v = y
        }
    }

    BENCHMARK("hybrid (ZE)")
    {
        bvh::UVOverlapOptions opts;
        opts.method = bvh::UVOverlapMethod::Hybrid;
        return bvh::compute_uv_overlap(mesh, opts);
    };

    BENCHMARK("sweep-and-prune (ZE)")
    {
        bvh::UVOverlapOptions opts;
        opts.method = bvh::UVOverlapMethod::SweepAndPrune;
        return bvh::compute_uv_overlap(mesh, opts);
    };

    BENCHMARK("BVH per-triangle")
    {
        bvh::UVOverlapOptions opts;
        opts.method = bvh::UVOverlapMethod::BVH;
        return bvh::compute_uv_overlap(mesh, opts);
    };

    BENCHMARK("hybrid (ZE) + coloring")
    {
        bvh::UVOverlapOptions opts;
        opts.method = bvh::UVOverlapMethod::Hybrid;
        opts.compute_overlap_coloring = true;
        return bvh::compute_uv_overlap(mesh, opts);
    };
}

TEST_CASE("compute_uv_overlap benchmark: grenade", "[bvh][!benchmark]" LA_CORP_FLAG)
{
    using namespace lagrange;

    // Largest mesh in data/ with real UV coordinates (708 K triangles after triangulation).
    auto mesh = lagrange::testing::load_surface_mesh<Scalar, Index>("corp/io/Grenade_H.obj");
    lagrange::triangulate_polygonal_facets(mesh);
    la_runtime_assert(mesh.is_triangle_mesh());

    BENCHMARK("hybrid (ZE)")
    {
        bvh::UVOverlapOptions opts;
        opts.method = bvh::UVOverlapMethod::Hybrid;
        return bvh::compute_uv_overlap(mesh, opts);
    };

    BENCHMARK("sweep-and-prune (ZE)")
    {
        bvh::UVOverlapOptions opts;
        opts.method = bvh::UVOverlapMethod::SweepAndPrune;
        return bvh::compute_uv_overlap(mesh, opts);
    };

    BENCHMARK("BVH per-triangle")
    {
        bvh::UVOverlapOptions opts;
        opts.method = bvh::UVOverlapMethod::BVH;
        return bvh::compute_uv_overlap(mesh, opts);
    };

    BENCHMARK("hybrid (ZE) + coloring")
    {
        bvh::UVOverlapOptions opts;
        opts.method = bvh::UVOverlapMethod::Hybrid;
        opts.compute_overlap_coloring = true;
        return bvh::compute_uv_overlap(mesh, opts);
    };
}
