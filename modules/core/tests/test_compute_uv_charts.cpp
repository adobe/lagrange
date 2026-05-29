/*
 * Copyright 2025 Adobe. All rights reserved.
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

#include <lagrange/Attribute.h>
#include <lagrange/IndexedAttribute.h>
#include <lagrange/compute_uv_charts.h>
#include <lagrange/compute_uv_orientation.h>
#include <lagrange/unflip_uv_charts.h>
#include <lagrange/views.h>

using namespace lagrange;

TEST_CASE("compute_uv_charts", "[surface][utilities]")
{
    using Scalar = double;
    using Index = uint32_t;

    SurfaceMesh<Scalar, Index> mesh;
    mesh.add_vertex({0, 0, 0});
    mesh.add_vertex({1, 0, 0});
    mesh.add_vertex({0, 1, 0});
    mesh.add_vertex({1, 1, 0});
    mesh.add_triangle(0, 1, 2);
    mesh.add_triangle(1, 3, 2);

    SECTION("Single chart")
    {
        std::vector<Scalar> uv_values = {0, 0, 1, 0, 0, 1, 1, 1};
        std::vector<Index> uv_indices = {0, 1, 2, 1, 3, 2};

        mesh.template create_attribute<Scalar>(
            "uv",
            AttributeElement::Indexed,
            AttributeUsage::UV,
            2,
            {uv_values.data(), uv_values.size()},
            {uv_indices.data(), uv_indices.size()});

        auto num_charts = compute_uv_charts(mesh);
        REQUIRE(num_charts == 1);
    }

    SECTION("Two charts")
    {
        std::vector<Scalar> uv_values = {0, 0, 1, 0, 0, 1, 1, 0, 1, 1, 0, 1};
        std::vector<Index> uv_indices = {0, 1, 2, 3, 4, 5};

        mesh.template create_attribute<Scalar>(
            "uv",
            AttributeElement::Indexed,
            AttributeUsage::UV,
            2,
            {uv_values.data(), uv_values.size()},
            {uv_indices.data(), uv_indices.size()});

        auto num_charts = compute_uv_charts(mesh);
        REQUIRE(num_charts == 2);
    }
}

TEST_CASE("compute_uv_charts: different UV scalar type", "[surface][utilities]")
{
    using Scalar = double;
    using Index = uint32_t;
    using UVScalar = float;

    SurfaceMesh<Scalar, Index> mesh;
    mesh.add_vertex({0, 0, 0});
    mesh.add_vertex({1, 0, 0});
    mesh.add_vertex({0, 1, 0});
    mesh.add_vertex({1, 1, 0});
    mesh.add_triangle(0, 1, 2);
    mesh.add_triangle(1, 3, 2);

    SECTION("Single chart with float UVs on double mesh")
    {
        std::vector<UVScalar> uv_values = {0, 0, 1, 0, 0, 1, 1, 1};
        std::vector<Index> uv_indices = {0, 1, 2, 1, 3, 2};

        mesh.template create_attribute<UVScalar>(
            "uv",
            AttributeElement::Indexed,
            AttributeUsage::UV,
            2,
            {uv_values.data(), uv_values.size()},
            {uv_indices.data(), uv_indices.size()});

        auto num_charts = compute_uv_charts(mesh);
        REQUIRE(num_charts == 1);
    }

    SECTION("Two charts with float UVs on double mesh")
    {
        std::vector<UVScalar> uv_values = {0, 0, 1, 0, 0, 1, 1, 0, 1, 1, 0, 1};
        std::vector<Index> uv_indices = {0, 1, 2, 3, 4, 5};

        mesh.template create_attribute<UVScalar>(
            "uv",
            AttributeElement::Indexed,
            AttributeUsage::UV,
            2,
            {uv_values.data(), uv_values.size()},
            {uv_indices.data(), uv_indices.size()});

        auto num_charts = compute_uv_charts(mesh);
        REQUIRE(num_charts == 2);
    }

    SECTION("Single chart with float vertex UVs on double mesh")
    {
        std::vector<UVScalar> uv_values = {0, 0, 1, 0, 0, 1, 1, 1};

        mesh.template create_attribute<UVScalar>(
            "uv",
            AttributeElement::Vertex,
            AttributeUsage::UV,
            2,
            uv_values);

        auto num_charts = compute_uv_charts(mesh);
        REQUIRE(num_charts == 1);
    }
}

TEST_CASE("compute_uv_orientation and unflip_uv_charts", "[surface][utilities]")
{
    using Scalar = double;
    using Index = uint32_t;

    SurfaceMesh<Scalar, Index> mesh;
    mesh.add_vertex({0, 0, 0});
    mesh.add_vertex({1, 0, 0});
    mesh.add_vertex({0, 1, 0});
    mesh.add_vertex({1, 1, 0});
    mesh.add_triangle(0, 1, 2);
    mesh.add_triangle(1, 3, 2);

    SECTION("All flipped")
    {
        std::vector<Scalar> uv_values = {0, 0, 0, 1, 1, 0, 1, 1};
        std::vector<Index> uv_indices = {0, 1, 2, 1, 3, 2};

        mesh.template create_attribute<Scalar>(
            "uv",
            AttributeElement::Indexed,
            AttributeUsage::UV,
            2,
            {uv_values.data(), uv_values.size()},
            {uv_indices.data(), uv_indices.size()});

        auto counts = compute_uv_orientation(mesh);
        REQUIRE(counts.negative == 2);
        REQUIRE(counts.positive == 0);

        size_t n_unflipped = unflip_uv_charts(mesh);
        REQUIRE(n_unflipped == 1);
        REQUIRE(compute_uv_orientation(mesh).negative == 0);

        // U should be negated for every UV vertex in the (single, flipped) chart;
        // V and corner indices should be unchanged.
        auto& uv_attr = mesh.template get_indexed_attribute<Scalar>("uv");
        auto post_values = matrix_view(uv_attr.values());
        auto post_indices = vector_view(uv_attr.indices());
        const std::array<std::array<Scalar, 2>, 4> expected_values = {
            {{0, 0}, {0, 1}, {-1, 0}, {-1, 1}}};
        for (Index v = 0; v < 4; ++v) {
            REQUIRE(post_values(v, 0) == expected_values[v][0]);
            REQUIRE(post_values(v, 1) == expected_values[v][1]);
        }
        const std::array<Index, 6> expected_indices = {0, 1, 2, 1, 3, 2};
        for (size_t i = 0; i < expected_indices.size(); ++i) {
            REQUIRE(post_indices[i] == expected_indices[i]);
        }
    }

    SECTION("Mixed: only flipped chart is unflipped")
    {
        // Chart A (UV verts 0..2, tri0) is CCW. Chart B (UV verts 3..5, tri1) is CW.
        std::vector<Scalar> uv_values = {0, 0, 1, 0, 0, 1, 2, 0, 2, 1, 3, 0};
        std::vector<Index> uv_indices = {0, 1, 2, 3, 4, 5};

        mesh.template create_attribute<Scalar>(
            "uv",
            AttributeElement::Indexed,
            AttributeUsage::UV,
            2,
            {uv_values.data(), uv_values.size()},
            {uv_indices.data(), uv_indices.size()});

        REQUIRE(compute_uv_orientation(mesh).negative == 1);
        REQUIRE(unflip_uv_charts(mesh) == 1);
        REQUIRE(compute_uv_orientation(mesh).negative == 0);

        // Chart A vertices (0..2) untouched; Chart B vertices (3..5) have negated U.
        auto& uv_attr = mesh.template get_indexed_attribute<Scalar>("uv");
        auto post = matrix_view(uv_attr.values());
        const std::array<std::array<Scalar, 2>, 6> expected = {
            {{0, 0}, {1, 0}, {0, 1}, {-2, 0}, {-2, 1}, {-3, 0}}};
        for (Index v = 0; v < 6; ++v) {
            REQUIRE(post(v, 0) == expected[v][0]);
            REQUIRE(post(v, 1) == expected[v][1]);
        }
    }
}
