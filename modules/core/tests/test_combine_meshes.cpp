/*
 * Copyright 2022 Adobe. All rights reserved.
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
#include <catch2/generators/catch_generators.hpp>

#include <lagrange/IndexedAttribute.h>
#include <lagrange/attribute_names.h>
#include <lagrange/combine_meshes.h>
#include <lagrange/compute_normal.h>
#include <lagrange/internal/constants.h>
#include <lagrange/map_attribute.h>
#include <lagrange/utils/range.h>
#include <lagrange/views.h>

#ifdef LAGRANGE_ENABLE_LEGACY_FUNCTIONS
    #include <lagrange/combine_mesh_list.h>
    #include <lagrange/mesh_convert.h>
#endif

#include <algorithm>
#include <array>
#include <numeric>
#include <string>

using namespace lagrange;

TEST_CASE("combine_meshes", "[surface][utilities]")
{
    using Scalar = double;
    using Index = uint32_t;

    SurfaceMesh<Scalar, Index> mesh1;
    mesh1.add_vertex({0, 0, 0});
    mesh1.add_vertex({1, 0, 0});
    mesh1.add_vertex({0, 1, 0});
    mesh1.add_triangle(0, 1, 2);

    SurfaceMesh<Scalar, Index> mesh2;
    mesh2.add_vertex({2, 0, 0});
    mesh2.add_vertex({2, 1, 0});
    mesh2.add_vertex({2, 0, 1});
    mesh2.add_vertex({2, 1, 1});
    mesh2.add_quad(0, 1, 3, 2);

    const Eigen::Matrix<Scalar, 1, 3> normal1(0, 0, 1);
    const Eigen::Matrix<Scalar, 1, 3> normal2(1, 0, 0);

    NormalOptions options;
    options.output_attribute_name = "normal";
    auto id1 = compute_normal(mesh1, lagrange::internal::pi / 4, {}, options);
    auto id2 = compute_normal(mesh2, lagrange::internal::pi / 4, {}, options);
    REQUIRE(mesh1.is_attribute_indexed(id1));
    REQUIRE(mesh2.is_attribute_indexed(id2));

    SECTION("without attributes")
    {
        auto out_mesh = combine_meshes<Scalar, Index>({&mesh1, &mesh2}, false);
        REQUIRE(out_mesh.get_num_vertices() == 7);
        REQUIRE(out_mesh.get_num_facets() == 2);
        REQUIRE(!out_mesh.has_attribute(options.output_attribute_name));
    }

    SECTION("with indexed attributes")
    {
        auto out_mesh = combine_meshes<Scalar, Index>({&mesh1, &mesh2}, true);
        REQUIRE(out_mesh.get_num_vertices() == 7);
        REQUIRE(out_mesh.get_num_facets() == 2);
        REQUIRE(out_mesh.has_attribute(options.output_attribute_name));
        REQUIRE(out_mesh.is_attribute_indexed(options.output_attribute_name));

        const auto& attr = out_mesh.get_indexed_attribute<Scalar>(options.output_attribute_name);
        const auto attr_value_view = matrix_view(attr.values());
        const auto attr_index_view = matrix_view(attr.indices());

        for (auto ci : range(static_cast<Index>(attr_index_view.rows()))) {
            Index i = attr_index_view(ci);
            Index fi = out_mesh.get_corner_facet(ci);
            if (fi == 0) {
                REQUIRE(attr_value_view.row(i) == normal1);
            } else {
                REQUIRE(attr_value_view.row(i) == normal2);
            }
        }
    }

    SECTION("with facet attributes")
    {
        constexpr char name[] = "facet_normal";
        map_attribute(mesh1, options.output_attribute_name, name, Facet);
        map_attribute(mesh2, options.output_attribute_name, name, Facet);
        auto out_mesh = combine_meshes<Scalar, Index>({&mesh1, &mesh2}, true);
        REQUIRE(out_mesh.get_num_vertices() == 7);
        REQUIRE(out_mesh.get_num_facets() == 2);
        REQUIRE(out_mesh.has_attribute(name));

        REQUIRE(out_mesh.template is_attribute_type<Scalar>(name));
        const auto& attr = out_mesh.get_attribute<Scalar>(name);
        auto attr_view = matrix_view(attr);
        REQUIRE(attr_view.rows() == 2);
        REQUIRE(attr_view.cols() == 3);

        REQUIRE(attr_view.row(0) == normal1);
        REQUIRE(attr_view.row(1) == normal2);
    }

    SECTION("with corner attributes")
    {
        constexpr char name[] = "corner_normal";
        map_attribute(mesh1, options.output_attribute_name, name, Corner);
        map_attribute(mesh2, options.output_attribute_name, name, Corner);
        auto out_mesh = combine_meshes<Scalar, Index>({&mesh1, &mesh2}, true);
        REQUIRE(out_mesh.get_num_vertices() == 7);
        REQUIRE(out_mesh.get_num_facets() == 2);
        REQUIRE(out_mesh.has_attribute(name));

        REQUIRE(out_mesh.template is_attribute_type<Scalar>(name));
        const auto& attr = out_mesh.get_attribute<Scalar>(name);
        auto attr_view = matrix_view(attr);
        REQUIRE(attr_view.rows() == 7);
        REQUIRE(attr_view.cols() == 3);

        for (size_t i = 0; i < 3; i++) {
            REQUIRE(attr_view.row(i) == normal1);
        }
        for (size_t i = 3; i < 7; i++) {
            REQUIRE(attr_view.row(i) == normal2);
        }
    }

    SECTION("with edge attributes")
    {
        constexpr char name[] = "edge_normal";
        map_attribute(mesh1, options.output_attribute_name, name, Edge);
        map_attribute(mesh2, options.output_attribute_name, name, Edge);
        auto out_mesh = combine_meshes<Scalar, Index>({&mesh1, &mesh2}, true);
        REQUIRE(out_mesh.get_num_vertices() == 7);
        REQUIRE(out_mesh.get_num_facets() == 2);
        REQUIRE(out_mesh.has_attribute(name));

        REQUIRE(out_mesh.template is_attribute_type<Scalar>(name));
        const auto& attr = out_mesh.get_attribute<Scalar>(name);
        auto attr_view = matrix_view(attr);
        REQUIRE(attr_view.rows() == 7);
        REQUIRE(attr_view.cols() == 3);

        for (size_t i = 0; i < 3; i++) {
            REQUIRE(attr_view.row(i) == normal1);
        }
        for (size_t i = 3; i < 7; i++) {
            REQUIRE(attr_view.row(i) == normal2);
        }
    }

    SECTION("default policy concatenates indices")
    {
        CombineMeshesOptions combine_options;
        REQUIRE(combine_options.value_attribute_policy == ValueAttributePolicy::ConcatenateIndices);
        const Index refs[] = {0};
        mesh1.create_attribute<Index>("refs", Value, AttributeUsage::VertexIndex, 1, refs);
        mesh2.create_attribute<Index>("refs", Value, AttributeUsage::VertexIndex, 1, refs);
        auto default_mesh = combine_meshes<Scalar, Index>({&mesh1, &mesh2});
        auto legacy_mesh = combine_meshes<Scalar, Index>({&mesh1, &mesh2}, true);
        auto options_mesh = combine_meshes<Scalar, Index>({&mesh1, &mesh2}, combine_options);
        for (const auto* mesh : {&default_mesh, &legacy_mesh, &options_mesh}) {
            REQUIRE(mesh->template is_attribute_type<Index>("refs"));
            const auto& attr = mesh->template get_attribute<Index>("refs");
            REQUIRE(attr.get_num_elements() == 2);
            REQUIRE(attr.get_num_channels() == 1);
            REQUIRE(attr.get(0) == 0);
            REQUIRE(attr.get(1) == mesh1.get_num_vertices());
        }
        combine_options.value_attribute_policy = ValueAttributePolicy::RequireEqual;
        const std::array<SurfaceMesh<Scalar, Index>, 2> inputs = {mesh1, mesh2};
        auto span_mesh = combine_meshes<Scalar, Index>(inputs, combine_options);
        auto callback_mesh = combine_meshes<Scalar, Index>(
            inputs.size(),
            [&](size_t i) -> const SurfaceMesh<Scalar, Index>& { return inputs[i]; },
            combine_options);
        REQUIRE(span_mesh.template get_attribute<Index>("refs").get_num_elements() == 1);
        REQUIRE(callback_mesh.template get_attribute<Index>("refs").get_num_elements() == 1);
    }

    SECTION("with value attributes")
    {
        CombineMeshesOptions combine_options;
        combine_options.value_attribute_policy =
            GENERATE(ValueAttributePolicy::RequireEqual, ValueAttributePolicy::ConcatenateIndices);
        constexpr char name[] = "value_normal";
        map_attribute(mesh1, options.output_attribute_name, name, Value);
        mesh2.create_attribute_from(name, mesh1);
        bool identical = true;
        SECTION("identical") {}
        SECTION("identical independent storage")
        {
            const auto& other_attr = mesh2.ref_attribute<Scalar>(name);
            REQUIRE(
                other_attr.get_all().data() != mesh1.get_attribute<Scalar>(name).get_all().data());
        }
        SECTION("different sizes")
        {
            mesh2.ref_attribute<Scalar>(name).resize_elements(1);
            identical = false;
        }
        SECTION("different values")
        {
            mesh2.ref_attribute<Scalar>(name).ref_all().front() = 1;
            identical = false;
        }

        auto out_mesh = combine_meshes<Scalar, Index>({&mesh1, &mesh2}, combine_options);
        REQUIRE(out_mesh.get_num_vertices() == 7);
        REQUIRE(out_mesh.get_num_facets() == 2);
        REQUIRE(out_mesh.has_attribute(name) == identical);
        if (identical) {
            REQUIRE(out_mesh.template is_attribute_type<Scalar>(name));
            const auto& attr = out_mesh.get_attribute<Scalar>(name);
            const auto attr_view = matrix_view(attr);
            REQUIRE(attr_view.rows() == 3);
            REQUIRE(attr_view.cols() == 3);
            REQUIRE(attr_view == matrix_view(mesh1.get_attribute<Scalar>(name)));
        }
    }
}

TEST_CASE("combine_meshes initialized empty edges", "[surface][utilities]")
{
    CombineMeshesOptions options;
    options.value_attribute_policy =
        GENERATE(ValueAttributePolicy::RequireEqual, ValueAttributePolicy::ConcatenateIndices);
    SurfaceMesh32d points;
    points.add_vertices(3);
    points.initialize_edges();
    points.create_metadata(AttributeName::name, "Points");
    points.create_attribute<double>("edge_value", Edge, AttributeUsage::Scalar, 1);
    points.create_attribute<uint32_t>("edge_refs", Value, AttributeUsage::EdgeIndex, 1);
    REQUIRE(points.has_edges());
    REQUIRE(points.get_num_edges() == 0);

    auto combined = combine_meshes({&points, &points}, options);
    REQUIRE(combined.get_num_vertices() == 6);
    REQUIRE(combined.get_num_facets() == 0);
    REQUIRE(combined.get_num_corners() == 0);
    REQUIRE(combined.get_num_edges() == 0);
    REQUIRE(combined.has_edges());
    REQUIRE(combined.get_metadata(AttributeName::name) == "Points");
    REQUIRE(combined.get_attribute<double>("edge_value").get_num_elements() == 0);
    REQUIRE(combined.get_attribute<uint32_t>("edge_refs").get_num_elements() == 0);
    REQUIRE(points.has_edges());
    REQUIRE(points.get_num_edges() == 0);
    REQUIRE(points.get_num_facets() == 0);
}

TEST_CASE("combine_meshes metadata", "[surface][utilities]")
{
    SurfaceMesh32d mesh1;
    mesh1.add_vertices(3);
    mesh1.add_triangle(0, 1, 2);
    mesh1.create_metadata(AttributeName::name, "Cube");
    auto mesh2 = mesh1;
    bool preserve_attributes = true;
    bool preserve_name = true;
    SECTION("identical") {}
    SECTION("different lengths")
    {
        mesh2.set_metadata(AttributeName::name, "Other");
        preserve_name = false;
    }
    SECTION("different strings of the same length")
    {
        mesh2.set_metadata(AttributeName::name, "Ball");
        preserve_name = false;
    }
    SECTION("empty")
    {
        mesh1.set_metadata(AttributeName::name, "");
        mesh2.set_metadata(AttributeName::name, "");
    }
    SECTION("missing")
    {
        mesh2.delete_attribute(AttributeName::name);
        preserve_name = false;
    }
    SECTION("without attributes")
    {
        mesh2.set_metadata(AttributeName::name, "Other");
        preserve_attributes = false;
        preserve_name = false;
    }

    auto combined = combine_meshes({&mesh1, &mesh2}, preserve_attributes);
    REQUIRE(combined.get_num_vertices() == 6);
    REQUIRE(combined.get_num_facets() == 2);
    REQUIRE(combined.has_attribute(AttributeName::name) == preserve_name);
    if (preserve_name) {
        const std::string original_name(mesh1.get_metadata(AttributeName::name));
        REQUIRE(combined.get_metadata(AttributeName::name) == original_name);
        REQUIRE(
            &combined.get_attribute<uint8_t>(AttributeName::name) ==
            &mesh1.get_attribute<uint8_t>(AttributeName::name));
        combined.set_metadata(AttributeName::name, "Combined");
        REQUIRE(mesh1.get_metadata(AttributeName::name) == original_name);
        REQUIRE(mesh2.get_metadata(AttributeName::name) == original_name);
    }
}

TEST_CASE("combine_meshes Value index attribute policies", "[surface][utilities]")
{
    CombineMeshesOptions options;
    options.value_attribute_policy =
        GENERATE(ValueAttributePolicy::RequireEqual, ValueAttributePolicy::ConcatenateIndices);
    const auto usage = GENERATE(
        AttributeUsage::VertexIndex,
        AttributeUsage::FacetIndex,
        AttributeUsage::CornerIndex,
        AttributeUsage::EdgeIndex);
    const uint32_t second_value = GENERATE(0u, 1u);
    SurfaceMesh32d mesh1;
    mesh1.add_vertices(3);
    mesh1.add_triangle(0, 1, 2);
    const uint32_t edges[] = {2, 0, 0, 1, 1, 2};
    mesh1.initialize_edges(edges);
    SurfaceMesh32d mesh2;
    mesh2.add_vertices(4);
    mesh2.add_quad(0, 1, 2, 3);
    mesh2.add_triangle(0, 2, 3);
    mesh2.initialize_edges();
    const uint32_t first_values[] = {0};
    const uint32_t second_values[] = {second_value};
    mesh1.create_attribute<uint32_t>("refs", Value, usage, 1, first_values);
    mesh2.create_attribute<uint32_t>("refs", Value, usage, 1, second_values);
    SurfaceMesh32d empty;
    auto combined = combine_meshes({&empty, &mesh1, &mesh2}, options);
    const bool concatenate =
        options.value_attribute_policy == ValueAttributePolicy::ConcatenateIndices;
    REQUIRE(combined.has_attribute("refs") == (second_value == 0 || concatenate));
    if (combined.has_attribute("refs")) {
        REQUIRE(combined.is_attribute_type<uint32_t>("refs"));
        const auto& attr = combined.get_attribute<uint32_t>("refs");
        REQUIRE(attr.get_num_elements() == (concatenate ? 2 : 1));
        REQUIRE(attr.get_num_channels() == 1);
        REQUIRE(attr.get(0) == 0);
        if (usage == AttributeUsage::EdgeIndex) {
            REQUIRE(combined.has_edges());
            REQUIRE(combined.get_num_edges() == mesh1.get_num_edges() + mesh2.get_num_edges());
            const auto actual = combined.get_edge_vertices(attr.get(0));
            const auto expected = mesh1.get_edge_vertices(0);
            REQUIRE(actual[0] == expected[0]);
            REQUIRE(actual[1] == expected[1]);
        }
        if (concatenate) {
            uint32_t offset = 0;
            switch (usage) {
            case AttributeUsage::VertexIndex: offset = mesh1.get_num_vertices(); break;
            case AttributeUsage::FacetIndex: offset = mesh1.get_num_facets(); break;
            case AttributeUsage::CornerIndex: offset = mesh1.get_num_corners(); break;
            case AttributeUsage::EdgeIndex: offset = mesh1.get_num_edges(); break;
            default: FAIL("Unexpected index usage");
            }
            REQUIRE(attr.get(1) == offset + second_value);
            if (usage == AttributeUsage::EdgeIndex) {
                const auto actual = combined.get_edge_vertices(attr.get(1));
                const auto expected = mesh2.get_edge_vertices(second_value);
                REQUIRE(actual[0] == expected[0] + mesh1.get_num_vertices());
                REQUIRE(actual[1] == expected[1] + mesh1.get_num_vertices());
            }
        }
    }
}

TEST_CASE("combine_meshes edge attributes require connectivity", "[surface][utilities]")
{
    const bool initialize_first = GENERATE(false, true);
    const bool initialize_second = GENERATE(false, true);
    const std::array<bool, 2> initialized = {initialize_first, initialize_second};
    const uint32_t edges[] = {2, 0, 0, 1, 1, 2};
    std::array<SurfaceMesh32d, 2> meshes;
    for (size_t i = 0; i < meshes.size(); ++i) {
        auto& mesh = meshes[i];
        mesh.add_vertices(3);
        mesh.add_triangle(0, 1, 2);
        mesh.create_metadata(AttributeName::name, "Triangle");
        if (initialized[i]) mesh.initialize_edges(edges);
        auto id = mesh.create_attribute<double>("edge_value", Edge, AttributeUsage::Scalar, 1);
        auto data = mesh.ref_attribute<double>(id).ref_all();
        for (size_t e = 0; e < data.size(); ++e) data[e] = double(i * 10 + e);
    }
    auto combined = combine_meshes<double, uint32_t>(meshes);
    const bool all_have_edges = initialized[0] && initialized[1];
    REQUIRE(combined.get_num_vertices() == 6);
    REQUIRE(combined.get_num_facets() == 2);
    REQUIRE(combined.get_metadata(AttributeName::name) == "Triangle");
    REQUIRE(combined.has_attribute("edge_value") == all_have_edges);
    REQUIRE(combined.has_edges() == all_have_edges);
    if (all_have_edges) {
        REQUIRE(combined.get_num_edges() == 6);
        const auto values = vector_view(combined.get_attribute<double>("edge_value"));
        for (uint32_t i = 0; i < 2; ++i) {
            REQUIRE(
                values.segment(i * 3, 3) ==
                vector_view(meshes[i].get_attribute<double>("edge_value")));
            for (uint32_t e = 0; e < 3; ++e) {
                const auto actual = combined.get_edge_vertices(i * 3 + e);
                const auto wanted = meshes[i].get_edge_vertices(e);
                REQUIRE(actual[0] == wanted[0] + i * 3);
                REQUIRE(actual[1] == wanted[1] + i * 3);
            }
        }
    }
    for (size_t i = 0; i < meshes.size(); ++i) {
        REQUIRE(meshes[i].has_edges() == initialized[i]);
    }
}

TEST_CASE("combine_meshes rejects uninitialized EdgeIndex attributes", "[surface][utilities]")
{
    CombineMeshesOptions options;
    options.preserve_attributes = GENERATE(false, true);
    const auto element = GENERATE(Value, Vertex, Facet, Corner, Edge, Indexed);
    std::array<SurfaceMesh32d, 2> meshes;
    meshes[0].add_vertices(3);
    meshes[0].add_triangle(0, 1, 2);
    meshes[0].initialize_edges();
    meshes[1].create_attribute<uint32_t>("unshared_refs", element, AttributeUsage::EdgeIndex, 1);
    LA_REQUIRE_THROWS(combine_meshes<double, uint32_t>(meshes, options));
    REQUIRE(meshes[0].has_edges());
    REQUIRE_FALSE(meshes[1].has_edges());
}

TEST_CASE("combine_meshes with custom edges", "[surface][utilities]")
{
    using Scalar = double;
    using Index = uint32_t;

    SurfaceMesh<Scalar, Index> mesh1;
    mesh1.add_vertex({0, 0, 0});
    mesh1.add_vertex({1, 0, 0});
    mesh1.add_vertex({0, 1, 0});
    mesh1.add_triangle(0, 1, 2);
    const std::vector<std::array<Index, 2>> edges1 = {{
        {{2, 0}},
        {{1, 2}},
        {{0, 1}},
    }};
    mesh1.initialize_edges({&edges1[0][0], 2 * edges1.size()});

    SurfaceMesh<Scalar, Index> mesh2;
    mesh2.add_vertex({2, 0, 0});
    mesh2.add_vertex({2, 1, 0});
    mesh2.add_vertex({2, 0, 1});
    mesh2.add_vertex({2, 1, 1});
    mesh2.add_quad(0, 1, 3, 2);
    const std::vector<std::array<Index, 2>> edges2 = {{
        {{1, 3}},
        {{0, 1}},
        {{3, 2}},
        {{2, 0}},
    }};
    mesh2.initialize_edges({&edges2[0][0], 2 * edges2.size()});

    for (auto* mesh_ptr : {&mesh1, &mesh2}) {
        auto& mesh = *mesh_ptr;
        auto id0 =
            mesh.create_attribute<Index>("v0", AttributeElement::Edge, AttributeUsage::VertexIndex);
        auto id1 =
            mesh.create_attribute<Index>("v1", AttributeElement::Edge, AttributeUsage::VertexIndex);
        auto v0 = vector_ref(mesh.ref_attribute<Index>(id0));
        auto v1 = vector_ref(mesh.ref_attribute<Index>(id1));
        for (Index e = 0; e < mesh.get_num_edges(); ++e) {
            auto v = mesh.get_edge_vertices(e);
            v0[e] = v[0];
            v1[e] = v[1];
            logger().info("e{}: {}, {}", e, v[0], v[1]);
        }
    }

    auto out_mesh = combine_meshes<Scalar, Index>({&mesh1, &mesh2}, true);

    auto v0 = vector_view(out_mesh.get_attribute<Index>("v0"));
    auto v1 = vector_view(out_mesh.get_attribute<Index>("v1"));
    for (Index e = 0; e < out_mesh.get_num_edges(); ++e) {
        auto v = out_mesh.get_edge_vertices(e);
        REQUIRE(v0[e] == v[0]);
        REQUIRE(v1[e] == v[1]);
    }
}

TEST_CASE("combine_meshes with indices", "[surface][utilities]")
{
    using Scalar = double;
    using Index = uint32_t;

    auto num_elements_from_usage = [](auto&& mesh, AttributeUsage usage) {
        switch (usage) {
        case AttributeUsage::VertexIndex: return mesh.get_num_vertices();
        case AttributeUsage::FacetIndex: return mesh.get_num_facets();
        case AttributeUsage::CornerIndex: return mesh.get_num_corners();
        case AttributeUsage::EdgeIndex: return mesh.get_num_edges();
        default: return Index(0);
        }
    };

    auto element_from_usage = [](AttributeUsage usage) {
        switch (usage) {
        case AttributeUsage::VertexIndex: return AttributeElement::Vertex;
        case AttributeUsage::FacetIndex: return AttributeElement::Facet;
        case AttributeUsage::CornerIndex: return AttributeElement::Corner;
        case AttributeUsage::EdgeIndex: return AttributeElement::Edge;
        default: return AttributeElement::Value;
        }
    };

    auto name_from_usage = [](AttributeUsage usage) {
        switch (usage) {
        case AttributeUsage::VertexIndex: return "VertexIndex";
        case AttributeUsage::FacetIndex: return "FacetIndex";
        case AttributeUsage::CornerIndex: return "CornerIndex";
        case AttributeUsage::EdgeIndex: return "EdgeIndex";
        default: return "";
        }
    };

    SurfaceMesh<Scalar, Index> mesh1;
    mesh1.add_vertex({0, 0, 0});
    mesh1.add_vertex({1, 0, 0});
    mesh1.add_vertex({0, 1, 0});
    mesh1.add_triangle(0, 1, 2);
    mesh1.initialize_edges();

    SurfaceMesh<Scalar, Index> mesh2;
    mesh2.add_vertex({2, 0, 0});
    mesh2.add_vertex({2, 1, 0});
    mesh2.add_vertex({2, 0, 1});
    mesh2.add_vertex({2, 1, 1});
    mesh2.add_quad(0, 1, 3, 2);
    mesh2.initialize_edges();

    auto usages = {
        AttributeUsage::VertexIndex,
        AttributeUsage::FacetIndex,
        AttributeUsage::CornerIndex,
        AttributeUsage::EdgeIndex,
    };

    for (auto* mesh_ptr : {&mesh1, &mesh2}) {
        auto& mesh = *mesh_ptr;
        for (auto usage : usages) {
            auto id = mesh.create_attribute<Index>(
                name_from_usage(usage),
                element_from_usage(usage),
                usage);
            auto attr = mesh.ref_attribute<Index>(id).ref_all();
            auto num_elements = num_elements_from_usage(mesh, usage);
            REQUIRE(static_cast<Index>(attr.size()) == num_elements);
            std::iota(attr.begin(), attr.end(), Index(0));
        }
    }

    auto out_mesh = combine_meshes<Scalar, Index>({&mesh1, &mesh2}, true);

    for (auto usage : usages) {
        auto attr = out_mesh.get_attribute<Index>(name_from_usage(usage)).get_all();
        auto num_elements = num_elements_from_usage(out_mesh, usage);
        REQUIRE(static_cast<Index>(attr.size()) == num_elements);
        std::vector<Index> expected(num_elements);
        std::iota(expected.begin(), expected.end(), Index(0));
        CAPTURE(name_from_usage(usage), attr);
        REQUIRE(std::equal(attr.begin(), attr.end(), expected.begin()));
    }
}

TEST_CASE("combine_meshes dimensions", "[surface][utilities]")
{
    using Scalar = double;
    using Index = uint32_t;

    SurfaceMesh<Scalar, Index> mesh2(2);
    mesh2.add_vertices(3);
    mesh2.add_triangle(0, 1, 2);

    SurfaceMesh<Scalar, Index> mesh3(3);
    mesh3.add_vertices(4);
    mesh3.add_quad(0, 1, 3, 2);

    SurfaceMesh<Scalar, Index> mesh4(4);
    mesh4.add_vertices(3);
    mesh4.add_triangle(0, 1, 2);

    // Same dimension: should be ok
    REQUIRE_NOTHROW(combine_meshes<Scalar, Index>({&mesh2, &mesh2}));
    REQUIRE_NOTHROW(combine_meshes<Scalar, Index>({&mesh3, &mesh3}));
    REQUIRE_NOTHROW(combine_meshes<Scalar, Index>({&mesh4, &mesh4}));

    // Different dimensions: kaboom
    LA_REQUIRE_THROWS(combine_meshes<Scalar, Index>({&mesh2, &mesh3}));
    LA_REQUIRE_THROWS(combine_meshes<Scalar, Index>({&mesh3, &mesh3, &mesh4}));
}

TEST_CASE("combine_meshes hybrid", "[surface][utilities]")
{
    using Scalar = double;
    using Index = uint32_t;

    auto mesh =
        lagrange::testing::load_surface_mesh<Scalar, Index>("open/core/poly/mixedFaringPart.obj");
    mesh = lagrange::combine_meshes({&mesh, &mesh}, false);
    REQUIRE(mesh.get_num_vertices() == 460);
    REQUIRE(mesh.get_num_facets() == 392);
}

TEST_CASE("combine_meshes empty", "[surface][utilities]")
{
    using Scalar = double;
    using Index = uint32_t;

    SurfaceMesh<Scalar, Index> mesh2;
    mesh2.add_vertices(3);
    mesh2.add_triangle(0, 1, 2);

    SurfaceMesh<Scalar, Index> mesh3;
    mesh3.add_vertices(4);
    mesh3.add_quad(0, 1, 3, 2);

    SurfaceMesh<Scalar, Index> empty;

    SECTION("all inputs empty")
    {
        for (bool preserve_attributes : {false, true}) {
            for (auto policy :
                 {ValueAttributePolicy::RequireEqual, ValueAttributePolicy::ConcatenateIndices}) {
                CombineMeshesOptions options;
                options.preserve_attributes = preserve_attributes;
                options.value_attribute_policy = policy;
                auto result = combine_meshes<Scalar, Index>({&empty, &empty}, options);
                REQUIRE(result.get_num_vertices() == 0);
                REQUIRE(result.get_num_facets() == 0);
                auto legacy = combine_meshes<Scalar, Index>({&empty}, preserve_attributes);
                REQUIRE(legacy.get_num_vertices() == 0);
                REQUIRE(legacy.get_num_facets() == 0);
            }
        }
    }

    SECTION("without attributes")
    {
        auto mesh0 = combine_meshes<Scalar, Index>({&mesh2, &mesh3, &empty}, true);
        auto mesh1 = combine_meshes<Scalar, Index>({&mesh2, &mesh3, &empty}, false);
        REQUIRE(mesh0.get_num_vertices() == 7);
        REQUIRE(mesh0.get_num_facets() == 2);
        REQUIRE(mesh1.get_num_vertices() == 7);
        REQUIRE(mesh1.get_num_facets() == 2);
    }

    SECTION("with attributes")
    {
        mesh2.create_attribute<Scalar>("x", AttributeElement::Facet, 3);
        mesh3.create_attribute<Scalar>("x", AttributeElement::Facet, 3);

        auto mesh0 = combine_meshes<Scalar, Index>({&mesh2, &mesh3, &empty}, true);
        auto mesh1 = combine_meshes<Scalar, Index>({&mesh2, &mesh3, &empty}, false);
        REQUIRE(mesh0.get_num_vertices() == 7);
        REQUIRE(mesh0.get_num_facets() == 2);
        REQUIRE(mesh1.get_num_vertices() == 7);
        REQUIRE(mesh1.get_num_facets() == 2);
        REQUIRE(mesh0.has_attribute("x"));
        REQUIRE(!mesh1.has_attribute("x"));
    }
}

TEST_CASE("combine_meshes benchmark", "[surface][utilities][!benchmark]")
{
    using namespace lagrange;
    using Scalar = double;
    using Index = uint32_t;

    auto mesh = lagrange::testing::load_surface_mesh<Scalar, Index>("open/core/dragon.obj");
    auto poly =
        lagrange::testing::load_surface_mesh<Scalar, Index>("open/core/poly/mixedFaringPart.obj");
    REQUIRE(mesh.has_attribute("normal"));

#ifdef LAGRANGE_ENABLE_LEGACY_FUNCTIONS
    using MeshType = TriangleMesh3D;
    auto legacy_mesh = to_legacy_mesh<MeshType>(mesh);

    std::vector<MeshType*> meshes;
    meshes.push_back(legacy_mesh.get());
    meshes.push_back(legacy_mesh.get());
#endif

    SECTION("without attributes")
    {
        BENCHMARK("combine_meshes without attributes")
        {
            return combine_meshes({&mesh, &mesh}, false);
        };
        BENCHMARK("combine_meshes mixed no attributes")
        {
            return combine_meshes({&mesh, &poly, &mesh}, false);
        };

#ifdef LAGRANGE_ENABLE_LEGACY_FUNCTIONS
        BENCHMARK("legacy::combine_mesh_list without attributes")
        {
            return legacy::combine_mesh_list(meshes, false);
        };
#endif
    }

    SECTION("with attributes")
    {
        BENCHMARK("combine_meshes with attributes")
        {
            return combine_meshes({&mesh, &mesh}, true);
        };
#ifdef LAGRANGE_ENABLE_LEGACY_FUNCTIONS
        BENCHMARK("legacy::combine_mesh_list with attributes")
        {
            return legacy::combine_mesh_list(meshes, true);
        };
#endif
    }
}
