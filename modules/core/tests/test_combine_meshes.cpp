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

#include <lagrange/IndexedAttribute.h>
#include <lagrange/combine_meshes.h>
#include <lagrange/compute_normal.h>
#include <lagrange/map_attribute.h>
#include <lagrange/views.h>

#ifdef LAGRANGE_ENABLE_LEGACY_FUNCTIONS
    #include <lagrange/combine_mesh_list.h>
    #include <lagrange/mesh_convert.h>
#endif

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
    auto id1 = compute_normal(mesh1, M_PI / 4, {}, options);
    auto id2 = compute_normal(mesh2, M_PI / 4, {}, options);
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

    SECTION("with value attribute")
    {
        constexpr char name[] = "value_normal";
        map_attribute(mesh1, options.output_attribute_name, name, Value);
        map_attribute(mesh2, options.output_attribute_name, name, Value);
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

TEST_CASE("combine_meshes hybrid", "[surface][utilities]")
{
    using Scalar = double;
    using Index = uint32_t;

    auto mesh =
        lagrange::testing::load_surface_mesh<Scalar, Index>("open/core/poly/mixedFaringPart.obj");
    mesh = lagrange::combine_meshes({&mesh, &mesh}, false);
    REQUIRE(mesh.get_num_vertices() == 464);
    REQUIRE(mesh.get_num_facets() == 408);
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

    using MeshType = TriangleMesh3D;
    auto legacy_mesh = to_legacy_mesh<MeshType>(mesh);

    std::vector<MeshType*> meshes;
    meshes.push_back(legacy_mesh.get());
    meshes.push_back(legacy_mesh.get());

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

        BENCHMARK("legacy::combine_mesh_list without attributes")
        {
            return legacy::combine_mesh_list(meshes, false);
        };
    }

    SECTION("with attributes")
    {
        BENCHMARK("combine_meshes with attributes")
        {
            return combine_meshes({&mesh, &mesh}, true);
        };
        BENCHMARK("legacy::combine_mesh_list with attributes")
        {
            return legacy::combine_mesh_list(meshes, true);
        };
    }
}
