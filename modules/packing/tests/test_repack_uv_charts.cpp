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
#include <lagrange/packing/repack_uv_charts.h>
#include <lagrange/views.h>

#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <array>
#include <cstdint>
#include <limits>
#include <string_view>
#include <vector>

using namespace lagrange;

namespace {

using Scalar = double;
using Index = uint32_t;

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

// Helper to get UV bounding box as {min_u, min_v, max_u, max_v}.
template <typename UVScalar = Scalar>
std::array<UVScalar, 4> get_uv_bbox(SurfaceMesh<Scalar, Index>& mesh, std::string_view name = "uv")
{
    auto& attr = mesh.template get_indexed_attribute<UVScalar>(mesh.get_attribute_id(name));
    auto values = matrix_view(attr.values());
    auto indices = attr.indices().get_all();
    UVScalar min_u = std::numeric_limits<UVScalar>::max();
    UVScalar min_v = std::numeric_limits<UVScalar>::max();
    UVScalar max_u = std::numeric_limits<UVScalar>::lowest();
    UVScalar max_v = std::numeric_limits<UVScalar>::lowest();
    for (size_t i = 0; i < indices.size(); ++i) {
        auto idx = indices[i];
        min_u = std::min(min_u, values(idx, 0));
        min_v = std::min(min_v, values(idx, 1));
        max_u = std::max(max_u, values(idx, 0));
        max_v = std::max(max_v, values(idx, 1));
    }
    return {min_u, min_v, max_u, max_v};
}

} // namespace

TEST_CASE("repack_uv_charts: empty mesh", "[packing][repack]")
{
    SECTION("No vertices, no facets")
    {
        SurfaceMesh<Scalar, Index> mesh;
        add_indexed_uv(mesh, {}, {});

        packing::repack_uv_charts(mesh);
        CHECK(mesh.get_num_vertices() == 0);
        CHECK(mesh.get_num_facets() == 0);
    }

    SECTION("Vertices but no facets")
    {
        SurfaceMesh<Scalar, Index> mesh;
        mesh.add_vertex({0, 0, 0});
        mesh.add_vertex({1, 0, 0});
        mesh.add_vertex({0, 1, 0});

        add_indexed_uv(mesh, {0, 0, 1, 0, 0, 1}, {});

        packing::repack_uv_charts(mesh);
        CHECK(mesh.get_num_vertices() == 3);
        CHECK(mesh.get_num_facets() == 0);
    }
}

TEST_CASE("repack_uv_charts: single chart", "[packing][repack]")
{
    SECTION("Single triangle normalized to unit square")
    {
        SurfaceMesh<Scalar, Index> mesh;
        mesh.add_vertex({0, 0, 0});
        mesh.add_vertex({1, 0, 0});
        mesh.add_vertex({0, 1, 0});
        mesh.add_triangle(0, 1, 2);

        // UVs span [0,2] x [0,2], should be normalized into ~[0,1] (margin shrinks it slightly)
        add_indexed_uv(mesh, {0, 0, 2, 0, 0, 2}, {0, 1, 2});

        packing::repack_uv_charts(mesh);

        auto bbox = get_uv_bbox(mesh);
        CHECK_THAT(bbox[0], Catch::Matchers::WithinAbs(0.0, 1e-6));
        CHECK_THAT(bbox[1], Catch::Matchers::WithinAbs(0.0, 1e-6));
        // Packing margin (default 1e-3) shrinks the chart slightly
        CHECK(bbox[2] <= 1.0 + 1e-6);
        CHECK(bbox[3] <= 1.0 + 1e-6);
        CHECK(bbox[2] > 0.99);
        CHECK(bbox[3] > 0.99);
    }

    SECTION("Two triangles sharing an edge, single chart")
    {
        //  2---3
        //  |\ |
        //  | \|
        //  0---1
        SurfaceMesh<Scalar, Index> mesh;
        mesh.add_vertex({0, 0, 0});
        mesh.add_vertex({1, 0, 0});
        mesh.add_vertex({0, 1, 0});
        mesh.add_vertex({1, 1, 0});
        mesh.add_triangle(0, 1, 2);
        mesh.add_triangle(1, 3, 2);

        // UVs offset to [10,12] x [10,12]
        add_indexed_uv(mesh, {10, 10, 12, 10, 10, 12, 12, 12}, {0, 1, 2, 1, 3, 2});

        packing::repack_uv_charts(mesh);

        auto bbox = get_uv_bbox(mesh);
        CHECK_THAT(bbox[0], Catch::Matchers::WithinAbs(0.0, 1e-6));
        CHECK_THAT(bbox[1], Catch::Matchers::WithinAbs(0.0, 1e-6));
        CHECK(bbox[2] <= 1.0 + 1e-6);
        CHECK(bbox[3] <= 1.0 + 1e-6);
        CHECK(bbox[2] > 0.99);
        CHECK(bbox[3] > 0.99);
    }
}

TEST_CASE("repack_uv_charts: multiple charts", "[packing][repack]")
{
    SECTION("Two disconnected triangles repacked into unit square")
    {
        SurfaceMesh<Scalar, Index> mesh;
        mesh.add_vertex({0, 0, 0});
        mesh.add_vertex({1, 0, 0});
        mesh.add_vertex({0.5, 1, 0});
        mesh.add_vertex({2, 0, 0});
        mesh.add_vertex({3, 0, 0});
        mesh.add_vertex({2.5, 1, 0});
        mesh.add_triangle(0, 1, 2);
        mesh.add_triangle(3, 4, 5);

        // Two separate UV charts (no shared UV vertices or edges)
        add_indexed_uv(mesh, {0, 0, 1, 0, 0.5, 1, 2, 0, 3, 0, 2.5, 1}, {0, 1, 2, 3, 4, 5});

        packing::repack_uv_charts(mesh);

        auto bbox = get_uv_bbox(mesh);
        // All UVs should fit within [0,1] x [0,1]
        CHECK(bbox[0] >= -1e-6);
        CHECK(bbox[1] >= -1e-6);
        CHECK(bbox[2] <= 1.0 + 1e-6);
        CHECK(bbox[3] <= 1.0 + 1e-6);
    }

    SECTION("With pre-computed chart attribute")
    {
        // Two triangles sharing an edge, but forced into separate charts
        SurfaceMesh<Scalar, Index> mesh;
        mesh.add_vertex({0, 0, 0});
        mesh.add_vertex({1, 0, 0});
        mesh.add_vertex({0, 1, 0});
        mesh.add_vertex({1, 1, 0});
        mesh.add_triangle(0, 1, 2);
        mesh.add_triangle(1, 3, 2);

        add_indexed_uv(mesh, {0, 0, 1, 0, 0, 1, 1, 1}, {0, 1, 2, 1, 3, 2});

        // Force different chart ids
        mesh.template create_attribute<Index>(
            "@chart_id",
            AttributeElement::Facet,
            AttributeUsage::Scalar,
            1,
            std::vector<Index>{0, 1});

        packing::RepackOptions opts;
        opts.chart_attribute_name = "@chart_id";
        packing::repack_uv_charts(mesh, opts);

        auto bbox = get_uv_bbox(mesh);
        CHECK(bbox[0] >= -1e-6);
        CHECK(bbox[1] >= -1e-6);
        CHECK(bbox[2] <= 1.0 + 1e-6);
        CHECK(bbox[3] <= 1.0 + 1e-6);
    }
}

TEST_CASE("repack_uv_charts: normalize option", "[packing][repack]")
{
    SECTION("normalize=false preserves original chart scale")
    {
        SurfaceMesh<Scalar, Index> mesh;
        mesh.add_vertex({0, 0, 0});
        mesh.add_vertex({1, 0, 0});
        mesh.add_vertex({0, 1, 0});
        mesh.add_triangle(0, 1, 2);

        // Chart bbox is 5x5; with normalize=false it must remain that size.
        add_indexed_uv(mesh, {0, 0, 5, 0, 0, 5}, {0, 1, 2});

        packing::RepackOptions opts;
        opts.normalize = false;
        packing::repack_uv_charts(mesh, opts);

        auto bbox = get_uv_bbox(mesh);
        // Min still shifted to origin even when normalization is disabled.
        CHECK_THAT(bbox[0], Catch::Matchers::WithinAbs(0.0, 1e-6));
        CHECK_THAT(bbox[1], Catch::Matchers::WithinAbs(0.0, 1e-6));
        CHECK_THAT(bbox[2] - bbox[0], Catch::Matchers::WithinAbs(5.0, 1e-6));
        CHECK_THAT(bbox[3] - bbox[1], Catch::Matchers::WithinAbs(5.0, 1e-6));
    }

    SECTION("normalize=true (default) rescales to unit box")
    {
        SurfaceMesh<Scalar, Index> mesh;
        mesh.add_vertex({0, 0, 0});
        mesh.add_vertex({1, 0, 0});
        mesh.add_vertex({0, 1, 0});
        mesh.add_triangle(0, 1, 2);

        add_indexed_uv(mesh, {0, 0, 5, 0, 0, 5}, {0, 1, 2});

        packing::RepackOptions opts;
        opts.normalize = true;
        packing::repack_uv_charts(mesh, opts);

        auto bbox = get_uv_bbox(mesh);
        CHECK_THAT(bbox[0], Catch::Matchers::WithinAbs(0.0, 1e-6));
        CHECK_THAT(bbox[1], Catch::Matchers::WithinAbs(0.0, 1e-6));
        CHECK(bbox[2] <= 1.0 + 1e-6);
        CHECK(bbox[3] <= 1.0 + 1e-6);
        CHECK(bbox[2] > 0.99);
        CHECK(bbox[3] > 0.99);
    }
}

TEST_CASE("repack_uv_charts: different UV scalar type", "[packing][repack]")
{
    using UVScalar = float;

    SECTION("Float UVs on double mesh")
    {
        SurfaceMesh<Scalar, Index> mesh;
        mesh.add_vertex({0, 0, 0});
        mesh.add_vertex({1, 0, 0});
        mesh.add_vertex({0, 1, 0});
        mesh.add_triangle(0, 1, 2);

        add_indexed_uv<UVScalar>(mesh, {0.f, 0.f, 2.f, 0.f, 0.f, 2.f}, {0, 1, 2});

        packing::repack_uv_charts(mesh);

        auto bbox = get_uv_bbox<UVScalar>(mesh);
        CHECK_THAT(static_cast<double>(bbox[0]), Catch::Matchers::WithinAbs(0.0, 1e-5));
        CHECK_THAT(static_cast<double>(bbox[1]), Catch::Matchers::WithinAbs(0.0, 1e-5));
        CHECK(bbox[2] <= 1.0f + 1e-5f);
        CHECK(bbox[3] <= 1.0f + 1e-5f);
        CHECK(bbox[2] > 0.99f);
        CHECK(bbox[3] > 0.99f);
    }

    SECTION("Float UVs with two charts")
    {
        SurfaceMesh<Scalar, Index> mesh;
        mesh.add_vertex({0, 0, 0});
        mesh.add_vertex({1, 0, 0});
        mesh.add_vertex({0.5, 1, 0});
        mesh.add_vertex({2, 0, 0});
        mesh.add_vertex({3, 0, 0});
        mesh.add_vertex({2.5, 1, 0});
        mesh.add_triangle(0, 1, 2);
        mesh.add_triangle(3, 4, 5);

        add_indexed_uv<UVScalar>(
            mesh,
            {0.f, 0.f, 1.f, 0.f, 0.5f, 1.f, 2.f, 0.f, 3.f, 0.f, 2.5f, 1.f},
            {0, 1, 2, 3, 4, 5});

        packing::repack_uv_charts(mesh);

        auto bbox = get_uv_bbox<UVScalar>(mesh);
        CHECK(bbox[0] >= -1e-5f);
        CHECK(bbox[1] >= -1e-5f);
        CHECK(bbox[2] <= 1.0f + 1e-5f);
        CHECK(bbox[3] <= 1.0f + 1e-5f);
    }
}
