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
#include <lagrange/disconnect_uv_charts.h>
#include <lagrange/views.h>

#include <catch2/catch_approx.hpp>

#include <algorithm>
#include <set>
#include <vector>

using namespace lagrange;

namespace {

using Scalar = double;
using Index = uint32_t;

// Helper to create an indexed UV attribute on a mesh.
template <typename UVScalar = Scalar>
void add_indexed_uv(
    SurfaceMesh<Scalar, Index>& mesh,
    std::vector<UVScalar> uv_values,
    std::vector<Index> uv_indices,
    std::string_view name = "uv")
{
    mesh.template create_attribute<UVScalar>(
        name,
        AttributeElement::Indexed,
        AttributeUsage::UV,
        2,
        {uv_values.data(), uv_values.size()},
        {uv_indices.data(), uv_indices.size()});
}

// Helper to read back UV indices from the mesh.
template <typename UVScalar = Scalar>
std::vector<Index> get_uv_indices(SurfaceMesh<Scalar, Index>& mesh, std::string_view name = "uv")
{
    auto& attr = mesh.template get_indexed_attribute<UVScalar>(mesh.get_attribute_id(name));
    auto indices = attr.indices().get_all();
    return {indices.begin(), indices.end()};
}

// Helper to get the number of UV values.
template <typename UVScalar = Scalar>
size_t get_num_uv_values(SurfaceMesh<Scalar, Index>& mesh, std::string_view name = "uv")
{
    auto& attr = mesh.template get_indexed_attribute<UVScalar>(mesh.get_attribute_id(name));
    return attr.values().get_num_elements();
}

} // namespace

TEST_CASE("disconnect_uv_charts", "[surface][utilities]")
{
    SECTION("Single chart - no duplicates")
    {
        // Two triangles sharing an edge, single UV chart.
        //
        //  2---3
        //  |\ |
        //  | \|
        //  0---1
        //
        SurfaceMesh<Scalar, Index> mesh;
        mesh.add_vertex({0, 0, 0});
        mesh.add_vertex({1, 0, 0});
        mesh.add_vertex({0, 1, 0});
        mesh.add_vertex({1, 1, 0});
        mesh.add_triangle(0, 1, 2);
        mesh.add_triangle(1, 3, 2);

        add_indexed_uv(mesh, {0, 0, 1, 0, 0, 1, 1, 1}, {0, 1, 2, 1, 3, 2});

        auto num_duped = disconnect_uv_charts(mesh);
        CHECK(num_duped == 0);
        CHECK(get_num_uv_values(mesh) == 4);
    }

    SECTION("Two separate charts - no shared vertices")
    {
        // Two triangles with completely separate UV indices (no shared UV vertex).
        SurfaceMesh<Scalar, Index> mesh;
        mesh.add_vertex({0, 0, 0});
        mesh.add_vertex({1, 0, 0});
        mesh.add_vertex({0, 1, 0});
        mesh.add_vertex({1, 1, 0});
        mesh.add_triangle(0, 1, 2);
        mesh.add_triangle(1, 3, 2);

        // 6 unique UV values, no sharing between triangles.
        add_indexed_uv(mesh, {0, 0, 1, 0, 0, 1, 0.5, 0, 1, 0.5, 0.5, 1}, {0, 1, 2, 3, 4, 5});

        auto num_duped = disconnect_uv_charts(mesh);
        CHECK(num_duped == 0);
        CHECK(get_num_uv_values(mesh) == 6);
    }

    SECTION("Two charts sharing a UV vertex (pinch point)")
    {
        // Four triangles arranged as a bowtie in UV space:
        //
        //     1          3
        //    /|\        /|
        //   / | \      / |
        //  0--+--2    /  |
        //     |\ |   /   |
        //     | \|  /    |
        //     4--+--5    |
        //         \      |
        //          ------4
        //
        // Triangles (0,1,2) and (0,4,1) share edge 0-1 -> same chart A
        // Triangles (2,3,5) and (2,5,4) share edge 2-5 -> same chart B
        // UV vertex 2 is shared between chart A and chart B (pinch point).
        //
        // Simplified: 2 triangles touching at a single UV vertex.
        SurfaceMesh<Scalar, Index> mesh;
        mesh.add_vertex({0, 0, 0});
        mesh.add_vertex({1, 0, 0});
        mesh.add_vertex({0.5, 0.5, 0}); // shared geometry vertex
        mesh.add_vertex({0, 1, 0});
        mesh.add_vertex({1, 1, 0});
        mesh.add_triangle(0, 1, 2);
        mesh.add_triangle(2, 3, 4);

        // UV vertex 2 is shared between triangles in different charts.
        // Triangle 0 corners: UV 0, 1, 2
        // Triangle 1 corners: UV 2, 3, 4
        // Charts: {tri0} and {tri1} (no shared edge in UV space).
        add_indexed_uv(mesh, {0, 0, 1, 0, 0.5, 0.5, 0, 1, 1, 1}, {0, 1, 2, 2, 3, 4});

        auto num_duped = disconnect_uv_charts(mesh);
        CHECK(num_duped == 1); // UV vertex 2 is duplicated
        CHECK(get_num_uv_values(mesh) == 6); // 5 original + 1 duplicate

        // Check that the two triangles no longer share any UV index.
        auto indices = get_uv_indices(mesh);
        // Triangle 0: indices[0..2], Triangle 1: indices[3..5]
        std::set<Index> tri0_uvs(indices.begin(), indices.begin() + 3);
        std::set<Index> tri1_uvs(indices.begin() + 3, indices.begin() + 6);
        std::vector<Index> intersection;
        std::set_intersection(
            tri0_uvs.begin(),
            tri0_uvs.end(),
            tri1_uvs.begin(),
            tri1_uvs.end(),
            std::back_inserter(intersection));
        CHECK(intersection.empty());
    }

    SECTION("Three charts sharing a single UV vertex")
    {
        // Three triangles meeting at a single UV vertex (vertex 0), no shared edges.
        SurfaceMesh<Scalar, Index> mesh;
        mesh.add_vertex({0, 0, 0}); // shared
        mesh.add_vertex({1, 0, 0});
        mesh.add_vertex({0.5, 1, 0});
        mesh.add_vertex({-1, 0, 0});
        mesh.add_vertex({-0.5, 1, 0});
        mesh.add_vertex({0, -1, 0});
        mesh.add_vertex({1, -1, 0});
        mesh.add_triangle(0, 1, 2);
        mesh.add_triangle(0, 3, 4);
        mesh.add_triangle(0, 5, 6);

        // All three triangles share UV index 0, but have no shared edges -> 3 charts.
        add_indexed_uv(
            mesh,
            {0, 0, 1, 0, 0.5, 1, -1, 0, -0.5, 1, 0, -1, 1, -1},
            {0, 1, 2, 0, 3, 4, 0, 5, 6});

        auto num_duped = disconnect_uv_charts(mesh);
        CHECK(num_duped == 2); // vertex 0 duplicated for 2nd and 3rd chart
        CHECK(get_num_uv_values(mesh) == 9); // 7 original + 2 duplicates
    }

    SECTION("With pre-computed chart_id attribute")
    {
        // Two triangles sharing an edge in UV space, but with an external chart_id
        // that forces them into separate charts.
        SurfaceMesh<Scalar, Index> mesh;
        mesh.add_vertex({0, 0, 0});
        mesh.add_vertex({1, 0, 0});
        mesh.add_vertex({0, 1, 0});
        mesh.add_vertex({1, 1, 0});
        mesh.add_triangle(0, 1, 2);
        mesh.add_triangle(1, 3, 2);

        // Shared edge (UV indices 1-2) between the two triangles.
        add_indexed_uv(mesh, {0, 0, 1, 0, 0, 1, 1, 1}, {0, 1, 2, 1, 3, 2});

        // Force different chart ids despite shared edge.
        mesh.template create_attribute<Index>(
            "@chart_id",
            AttributeElement::Facet,
            AttributeUsage::Scalar,
            1,
            std::vector<Index>{0, 1});

        DisconnectUVChartsOptions opts;
        opts.chart_id_attribute_name = "@chart_id";
        auto num_duped = disconnect_uv_charts(mesh, opts);
        CHECK(num_duped == 2); // UV vertices 1 and 2 are shared and must be duplicated
        CHECK(get_num_uv_values(mesh) == 6); // 4 original + 2 duplicates
    }

    SECTION("No-op on already separated charts")
    {
        // Two charts that don't share any UV vertex.
        SurfaceMesh<Scalar, Index> mesh;
        mesh.add_vertex({0, 0, 0});
        mesh.add_vertex({1, 0, 0});
        mesh.add_vertex({0.5, 1, 0});
        mesh.add_vertex({2, 0, 0});
        mesh.add_vertex({3, 0, 0});
        mesh.add_vertex({2.5, 1, 0});
        mesh.add_triangle(0, 1, 2);
        mesh.add_triangle(3, 4, 5);

        add_indexed_uv(mesh, {0, 0, 1, 0, 0.5, 1, 2, 0, 3, 0, 2.5, 1}, {0, 1, 2, 3, 4, 5});

        auto indices_before = get_uv_indices(mesh);
        auto num_duped = disconnect_uv_charts(mesh);
        CHECK(num_duped == 0);
        CHECK(get_uv_indices(mesh) == indices_before);
    }

    SECTION("UV values are preserved after duplication")
    {
        // Pinch point: check that the duplicated UV value matches the original.
        SurfaceMesh<Scalar, Index> mesh;
        mesh.add_vertex({0, 0, 0});
        mesh.add_vertex({1, 0, 0});
        mesh.add_vertex({0.5, 0.5, 0});
        mesh.add_vertex({0, 1, 0});
        mesh.add_vertex({1, 1, 0});
        mesh.add_triangle(0, 1, 2);
        mesh.add_triangle(2, 3, 4);

        const Scalar pinch_u = 0.5, pinch_v = 0.5;
        add_indexed_uv(mesh, {0, 0, 1, 0, pinch_u, pinch_v, 0, 1, 1, 1}, {0, 1, 2, 2, 3, 4});

        disconnect_uv_charts(mesh);

        // The duplicated vertex should have the same UV value as the original.
        auto& attr = mesh.template get_indexed_attribute<Scalar>(mesh.get_attribute_id("uv"));
        auto values = matrix_view(attr.values());
        auto indices = get_uv_indices(mesh);

        // Get the UV value referenced by each triangle at the pinch position.
        // Triangle 0, corner 2 and Triangle 1, corner 0 were both UV index 2.
        Index uv_idx_tri0 = indices[2];
        Index uv_idx_tri1 = indices[3];
        CHECK(uv_idx_tri0 != uv_idx_tri1); // They should now be different indices
        CHECK(values(uv_idx_tri0, 0) == Catch::Approx(pinch_u));
        CHECK(values(uv_idx_tri0, 1) == Catch::Approx(pinch_v));
        CHECK(values(uv_idx_tri1, 0) == Catch::Approx(pinch_u));
        CHECK(values(uv_idx_tri1, 1) == Catch::Approx(pinch_v));
    }

    SECTION("Multiple facets per chart sharing a duplicated vertex")
    {
        // Two charts, each with 2 triangles. UV vertex 2 is the pinch point shared by both charts.
        // Chart A: triangles 0,1 (share edge 1-2)
        // Chart B: triangles 2,3 (share edge 2-3)
        // Both charts reference UV vertex 2.
        SurfaceMesh<Scalar, Index> mesh;
        mesh.add_vertex({0, 0, 0});
        mesh.add_vertex({1, 0, 0});
        mesh.add_vertex({0.5, 0.5, 0}); // pinch point
        mesh.add_vertex({0, 1, 0});
        mesh.add_vertex({1, 1, 0});
        mesh.add_vertex({-1, 0, 0});
        mesh.add_vertex({2, 1, 0});
        // Chart A: tri(0,1,2) and tri(0,2,5) — connected via edge 0-2
        mesh.add_triangle(0, 1, 2);
        mesh.add_triangle(0, 2, 5);
        // Chart B: tri(2,3,4) and tri(2,4,6) — connected via edge 2-4
        mesh.add_triangle(2, 3, 4);
        mesh.add_triangle(2, 4, 6);

        // UV: vertex 2 shared across charts A and B, but no shared edges between charts.
        add_indexed_uv(
            mesh,
            {0, 0, 1, 0, 0.5, 0.5, 0, 1, 1, 1, -1, 0, 2, 1},
            {0, 1, 2, 0, 2, 5, 2, 3, 4, 2, 4, 6});

        auto num_duped = disconnect_uv_charts(mesh);
        CHECK(num_duped == 1); // Only one duplicate for vertex 2 (not one per corner)
        CHECK(get_num_uv_values(mesh) == 8); // 7 original + 1 duplicate

        // All corners in chart B referencing the pinch point should use the same new index.
        auto indices = get_uv_indices(mesh);
        // tri2 corner0 and tri3 corner0 both reference the duplicated vertex 2
        CHECK(indices[6] == indices[9]);
        // And they should differ from chart A's references to vertex 2
        CHECK(indices[2] != indices[6]);
    }

    SECTION("Bowtie within a single user-supplied chart")
    {
        // Two triangles touching at UV vertex 2 only (no shared UV edge). The user labels both
        // facets as the same chart, so the chart-split alone would not duplicate vertex 2. The
        // bowtie pass must still split the pinch point so that the two wedges get independent
        // UV indices, leaving the UV mesh manifold for downstream consumers (e.g. repack).
        SurfaceMesh<Scalar, Index> mesh;
        mesh.add_vertex({0, 0, 0});
        mesh.add_vertex({1, 0, 0});
        mesh.add_vertex({0.5, 0.5, 0});
        mesh.add_vertex({0, 1, 0});
        mesh.add_vertex({1, 1, 0});
        mesh.add_triangle(0, 1, 2);
        mesh.add_triangle(2, 3, 4);

        add_indexed_uv(mesh, {0, 0, 1, 0, 0.5, 0.5, 0, 1, 1, 1}, {0, 1, 2, 2, 3, 4});

        mesh.template create_attribute<Index>(
            "@chart_id",
            AttributeElement::Facet,
            AttributeUsage::Scalar,
            1,
            std::vector<Index>{0, 0}); // both facets in the same chart

        DisconnectUVChartsOptions opts;
        opts.chart_id_attribute_name = "@chart_id";
        auto num_duped = disconnect_uv_charts(mesh, opts);
        CHECK(num_duped == 1);
        CHECK(get_num_uv_values(mesh) == 6);

        auto indices = get_uv_indices(mesh);
        // The two corners that previously shared UV index 2 must now reference different indices.
        CHECK(indices[2] != indices[3]);
    }

    SECTION("Multiple wedges within a single user-supplied chart")
    {
        // Four triangles meeting at UV vertex 0, forming two wedges of two triangles each:
        //   wedge A: triangles 0,1 connected via UV edge 0-2
        //   wedge B: triangles 2,3 connected via UV edge 0-5
        // Wedge A and wedge B share only UV vertex 0 (no UV edge between them).
        // With all facets labeled as chart 0, the bowtie pass should produce exactly one
        // duplicate of vertex 0 (the second wedge gets a new index, the first keeps the original).
        SurfaceMesh<Scalar, Index> mesh;
        mesh.add_vertex({0, 0, 0});
        mesh.add_vertex({1, 0, 0});
        mesh.add_vertex({1, 1, 0});
        mesh.add_vertex({0, 1, 0});
        mesh.add_vertex({-1, 0, 0});
        mesh.add_vertex({-1, -1, 0});
        mesh.add_vertex({0, -1, 0});
        mesh.add_triangle(0, 1, 2); // wedge A
        mesh.add_triangle(0, 2, 3); // wedge A
        mesh.add_triangle(0, 4, 5); // wedge B
        mesh.add_triangle(0, 5, 6); // wedge B

        add_indexed_uv(
            mesh,
            {0, 0, 1, 0, 1, 1, 0, 1, -1, 0, -1, -1, 0, -1},
            {0, 1, 2, 0, 2, 3, 0, 4, 5, 0, 5, 6});

        mesh.template create_attribute<Index>(
            "@chart_id",
            AttributeElement::Facet,
            AttributeUsage::Scalar,
            1,
            std::vector<Index>{0, 0, 0, 0});

        DisconnectUVChartsOptions opts;
        opts.chart_id_attribute_name = "@chart_id";
        auto num_duped = disconnect_uv_charts(mesh, opts);
        CHECK(num_duped == 1); // one extra wedge → one duplicate
        CHECK(get_num_uv_values(mesh) == 8);

        auto indices = get_uv_indices(mesh);
        // Wedge A facets (f0, f1) keep a single shared index for vertex 0.
        CHECK(indices[0] == indices[3]);
        // Wedge B facets (f2, f3) keep a single shared (different) index for vertex 0.
        CHECK(indices[6] == indices[9]);
        // Wedge A and wedge B must use different indices for vertex 0.
        CHECK(indices[0] != indices[6]);
    }

    SECTION("Bowtie split combined with chart split")
    {
        // Three triangles meeting at UV vertex 0 with no shared UV edges. The user supplies
        // chart_id = {0, 0, 1}. The first two facets are a single chart with an internal
        // bowtie (1 bowtie duplicate). The third facet is in a different chart and triggers a
        // chart-level duplicate (1 chart duplicate). Total: 2 duplicates of vertex 0.
        SurfaceMesh<Scalar, Index> mesh;
        mesh.add_vertex({0, 0, 0});
        mesh.add_vertex({1, 0, 0});
        mesh.add_vertex({0.5, 1, 0});
        mesh.add_vertex({-1, 0, 0});
        mesh.add_vertex({-0.5, 1, 0});
        mesh.add_vertex({0, -1, 0});
        mesh.add_vertex({1, -1, 0});
        mesh.add_triangle(0, 1, 2);
        mesh.add_triangle(0, 3, 4);
        mesh.add_triangle(0, 5, 6);

        add_indexed_uv(
            mesh,
            {0, 0, 1, 0, 0.5, 1, -1, 0, -0.5, 1, 0, -1, 1, -1},
            {0, 1, 2, 0, 3, 4, 0, 5, 6});

        mesh.template create_attribute<Index>(
            "@chart_id",
            AttributeElement::Facet,
            AttributeUsage::Scalar,
            1,
            std::vector<Index>{0, 0, 1});

        DisconnectUVChartsOptions opts;
        opts.chart_id_attribute_name = "@chart_id";
        auto num_duped = disconnect_uv_charts(mesh, opts);
        CHECK(num_duped == 2);
        CHECK(get_num_uv_values(mesh) == 9);

        auto indices = get_uv_indices(mesh);
        // All three facets must reference different indices for the formerly-shared vertex 0.
        CHECK(indices[0] != indices[3]);
        CHECK(indices[0] != indices[6]);
        CHECK(indices[3] != indices[6]);
    }

    SECTION("Bowtie pass leaves manifold input untouched")
    {
        // Two triangles connected by a real UV edge form a single wedge at every shared
        // vertex. The bowtie pass must not introduce spurious duplicates.
        SurfaceMesh<Scalar, Index> mesh;
        mesh.add_vertex({0, 0, 0});
        mesh.add_vertex({1, 0, 0});
        mesh.add_vertex({0, 1, 0});
        mesh.add_vertex({1, 1, 0});
        mesh.add_triangle(0, 1, 2);
        mesh.add_triangle(1, 3, 2);

        add_indexed_uv(mesh, {0, 0, 1, 0, 0, 1, 1, 1}, {0, 1, 2, 1, 3, 2});

        mesh.template create_attribute<Index>(
            "@chart_id",
            AttributeElement::Facet,
            AttributeUsage::Scalar,
            1,
            std::vector<Index>{0, 0});

        DisconnectUVChartsOptions opts;
        opts.chart_id_attribute_name = "@chart_id";
        auto num_duped = disconnect_uv_charts(mesh, opts);
        CHECK(num_duped == 0);
        CHECK(get_num_uv_values(mesh) == 4);
    }

    SECTION("Interleaved chart ordering")
    {
        // Facets from two charts are interleaved: A, B, A, B.
        // UV vertex 0 is shared across both charts.
        // This tests that switching back to chart A after seeing chart B still works correctly.
        SurfaceMesh<Scalar, Index> mesh;
        mesh.add_vertex({0, 0, 0}); // shared
        mesh.add_vertex({1, 0, 0});
        mesh.add_vertex({0.5, 1, 0});
        mesh.add_vertex({-1, 0, 0});
        mesh.add_vertex({-0.5, 1, 0});
        mesh.add_vertex({2, 0, 0});
        mesh.add_vertex({1.5, 1, 0});
        mesh.add_vertex({-2, 0, 0});
        mesh.add_vertex({-1.5, 1, 0});

        // Chart A: tri(0,1,2) and tri(0,5,6) — no shared edge, so 2 separate charts
        // unless we force them with a chart_id attribute.
        mesh.add_triangle(0, 1, 2); // f0: chart A
        mesh.add_triangle(0, 3, 4); // f1: chart B
        mesh.add_triangle(0, 5, 6); // f2: chart A
        mesh.add_triangle(0, 7, 8); // f3: chart B

        add_indexed_uv(
            mesh,
            {0, 0, 1, 0, 0.5, 1, -1, 0, -0.5, 1, 2, 0, 1.5, 1, -2, 0, -1.5, 1},
            {0, 1, 2, 0, 3, 4, 0, 5, 6, 0, 7, 8});

        // Force chart assignment: A=0, B=1, A=0, B=1
        mesh.template create_attribute<Index>(
            "@chart_id",
            AttributeElement::Facet,
            AttributeUsage::Scalar,
            1,
            std::vector<Index>{0, 1, 0, 1});

        DisconnectUVChartsOptions opts;
        opts.chart_id_attribute_name = "@chart_id";
        auto num_duped = disconnect_uv_charts(mesh, opts);
        // 1 chart-split duplicate (vertex 0 between chart A and chart B), plus 2 bowtie
        // duplicates (one within chart A for f0/f2, one within chart B for f1/f3).
        CHECK(num_duped == 3);
        CHECK(get_num_uv_values(mesh) == 12);

        auto indices = get_uv_indices(mesh);
        // After bowtie disconnection, all four facets should reference distinct indices for the
        // formerly-shared UV vertex 0.
        std::set<Index> v0_refs{indices[0], indices[3], indices[6], indices[9]};
        CHECK(v0_refs.size() == 4);
    }
}

TEST_CASE("disconnect_uv_charts: empty mesh", "[surface][utilities]")
{
    SECTION("No vertices, no facets")
    {
        SurfaceMesh<Scalar, Index> mesh;
        add_indexed_uv(mesh, {}, {});

        auto num_duped = disconnect_uv_charts(mesh);
        CHECK(num_duped == 0);
        CHECK(get_num_uv_values(mesh) == 0);
    }

    SECTION("Vertices but no facets")
    {
        SurfaceMesh<Scalar, Index> mesh;
        mesh.add_vertex({0, 0, 0});
        mesh.add_vertex({1, 0, 0});
        mesh.add_vertex({0, 1, 0});

        // Indexed UV with 3 values but no index entries (no facets -> no corners).
        // After separation, unreferenced UV values are compacted away.
        add_indexed_uv(mesh, {0, 0, 1, 0, 0, 1}, {});

        auto num_duped = disconnect_uv_charts(mesh);
        CHECK(num_duped == 0);
        CHECK(get_num_uv_values(mesh) == 0);
    }
}

TEST_CASE("disconnect_uv_charts: different UV scalar type", "[surface][utilities]")
{
    using UVScalar = float;

    SECTION("Single chart with float UVs - no duplicates")
    {
        SurfaceMesh<Scalar, Index> mesh;
        mesh.add_vertex({0, 0, 0});
        mesh.add_vertex({1, 0, 0});
        mesh.add_vertex({0, 1, 0});
        mesh.add_vertex({1, 1, 0});
        mesh.add_triangle(0, 1, 2);
        mesh.add_triangle(1, 3, 2);

        add_indexed_uv<UVScalar>(
            mesh,
            {0.f, 0.f, 1.f, 0.f, 0.f, 1.f, 1.f, 1.f},
            {0, 1, 2, 1, 3, 2});

        auto num_duped = disconnect_uv_charts(mesh);
        CHECK(num_duped == 0);
        CHECK(get_num_uv_values<UVScalar>(mesh) == 4);
    }

    SECTION("Two charts sharing a UV vertex (pinch point) with float UVs")
    {
        SurfaceMesh<Scalar, Index> mesh;
        mesh.add_vertex({0, 0, 0});
        mesh.add_vertex({1, 0, 0});
        mesh.add_vertex({0.5, 0.5, 0});
        mesh.add_vertex({0, 1, 0});
        mesh.add_vertex({1, 1, 0});
        mesh.add_triangle(0, 1, 2);
        mesh.add_triangle(2, 3, 4);

        add_indexed_uv<UVScalar>(
            mesh,
            {0.f, 0.f, 1.f, 0.f, 0.5f, 0.5f, 0.f, 1.f, 1.f, 1.f},
            {0, 1, 2, 2, 3, 4});

        auto num_duped = disconnect_uv_charts(mesh);
        CHECK(num_duped == 1);
        CHECK(get_num_uv_values<UVScalar>(mesh) == 6);

        auto indices = get_uv_indices<UVScalar>(mesh);
        std::set<Index> tri0_uvs(indices.begin(), indices.begin() + 3);
        std::set<Index> tri1_uvs(indices.begin() + 3, indices.begin() + 6);
        std::vector<Index> intersection;
        std::set_intersection(
            tri0_uvs.begin(),
            tri0_uvs.end(),
            tri1_uvs.begin(),
            tri1_uvs.end(),
            std::back_inserter(intersection));
        CHECK(intersection.empty());
    }

    SECTION("UV values preserved after duplication with float UVs")
    {
        SurfaceMesh<Scalar, Index> mesh;
        mesh.add_vertex({0, 0, 0});
        mesh.add_vertex({1, 0, 0});
        mesh.add_vertex({0.5, 0.5, 0});
        mesh.add_vertex({0, 1, 0});
        mesh.add_vertex({1, 1, 0});
        mesh.add_triangle(0, 1, 2);
        mesh.add_triangle(2, 3, 4);

        const UVScalar pinch_u = 0.5f, pinch_v = 0.5f;
        add_indexed_uv<UVScalar>(
            mesh,
            {0.f, 0.f, 1.f, 0.f, pinch_u, pinch_v, 0.f, 1.f, 1.f, 1.f},
            {0, 1, 2, 2, 3, 4});

        disconnect_uv_charts(mesh);

        auto& attr = mesh.template get_indexed_attribute<UVScalar>(mesh.get_attribute_id("uv"));
        auto values = matrix_view(attr.values());
        auto indices = get_uv_indices<UVScalar>(mesh);

        Index uv_idx_tri0 = indices[2];
        Index uv_idx_tri1 = indices[3];
        CHECK(uv_idx_tri0 != uv_idx_tri1);
        CHECK(values(uv_idx_tri0, 0) == Catch::Approx(pinch_u));
        CHECK(values(uv_idx_tri0, 1) == Catch::Approx(pinch_v));
        CHECK(values(uv_idx_tri1, 0) == Catch::Approx(pinch_u));
        CHECK(values(uv_idx_tri1, 1) == Catch::Approx(pinch_v));
    }
}
