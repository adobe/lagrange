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
#include <lagrange/Attribute.h>
#include <lagrange/common.h>
#include <lagrange/compute_facet_normal.h>
#include <lagrange/compute_vertex_normal.h>
#include <lagrange/compute_weighted_corner_normal.h>
#include <lagrange/find_matching_attributes.h>
#include <lagrange/foreach_attribute.h>
#include <lagrange/io/load_mesh_msh.h>
#include <lagrange/io/save_mesh_msh.h>
#include <lagrange/testing/check_mesh.h>
#include <lagrange/testing/common.h>
#include <lagrange/testing/equivalence_check.h>
#include <lagrange/utils/range.h>
#include <lagrange/views.h>

#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <limits>
#include <sstream>


#include <string>
#include <vector>

namespace {

struct MshBlockHeader
{
    int entity_dim = 0;
    int entity_tag = 0;
    int type_or_parametric = 0;
    size_t count = 0;
};

std::string msh_section(const std::string& text, const std::string& name)
{
    const std::string begin = "$" + name;
    const std::string end = "$End" + name;
    const auto section_begin = text.find(begin);
    const auto section_end = text.find(end);
    REQUIRE(section_begin != std::string::npos);
    REQUIRE(section_end != std::string::npos);
    REQUIRE(section_end > section_begin);
    return text.substr(section_begin + begin.size(), section_end - (section_begin + begin.size()));
}

std::vector<MshBlockHeader> msh_node_blocks(const std::string& text)
{
    std::istringstream input(msh_section(text, "Nodes"));
    size_t num_blocks = 0;
    size_t num_nodes = 0;
    size_t min_tag = 0;
    size_t max_tag = 0;
    input >> num_blocks >> num_nodes >> min_tag >> max_tag;
    std::vector<MshBlockHeader> blocks;
    for (size_t i = 0; i < num_blocks; ++i) {
        MshBlockHeader header;
        input >> header.entity_dim >> header.entity_tag >> header.type_or_parametric >>
            header.count;
        REQUIRE(input);
        for (size_t node = 0; node < header.count; ++node) {
            size_t tag = 0;
            input >> tag;
        }
        const size_t stride =
            header.type_or_parametric == 0 ? 3 : static_cast<size_t>(3 + header.entity_dim);
        for (size_t node = 0; node < header.count; ++node) {
            for (size_t value = 0; value < stride; ++value) {
                double coordinate = 0;
                input >> coordinate;
            }
        }
        blocks.push_back(header);
    }
    REQUIRE(input);
    return blocks;
}

std::vector<MshBlockHeader> msh_element_blocks(const std::string& text)
{
    std::istringstream input(msh_section(text, "Elements"));
    size_t num_blocks = 0;
    size_t num_elements = 0;
    size_t min_tag = 0;
    size_t max_tag = 0;
    input >> num_blocks >> num_elements >> min_tag >> max_tag;
    std::vector<MshBlockHeader> blocks;
    for (size_t i = 0; i < num_blocks; ++i) {
        MshBlockHeader header;
        input >> header.entity_dim >> header.entity_tag >> header.type_or_parametric >>
            header.count;
        REQUIRE(input);
        const int nodes = header.type_or_parametric == 1   ? 2
                          : header.type_or_parametric == 2 ? 3
                                                           : 4;
        for (size_t element = 0; element < header.count; ++element) {
            size_t tag = 0;
            input >> tag;
            for (int node = 0; node < nodes; ++node) {
                size_t vertex = 0;
                input >> vertex;
            }
        }
        blocks.push_back(header);
    }
    REQUIRE(input);
    return blocks;
}

} // namespace
TEST_CASE("load_msh_point_cloud", "[mesh][io][msh]")
{
    using namespace lagrange;
    using Scalar = double;
    using Index = uint32_t;

    SurfaceMesh<Scalar, Index> mesh;
    mesh.add_vertex({0, 0, 0});
    Scalar normal_data[] = {0, 0, 1};
    mesh.create_attribute<Scalar>(
        "normal",
        AttributeElement::Vertex,
        AttributeUsage::Normal,
        3,
        {normal_data, 3});

    std::stringstream data;
    io::SaveOptions save_options;
    save_options.encoding = io::FileEncoding::Ascii;
    REQUIRE_NOTHROW(io::save_mesh_msh(data, mesh, save_options));

    io::LoadOptions load_options;
    load_options.stitch_vertices = true;
    auto mesh2 = io::load_mesh_msh<SurfaceMesh<Scalar, Index>>(data, load_options);
    REQUIRE(mesh2.get_num_vertices() == 1);
    REQUIRE(mesh2.get_num_facets() == 0);
    REQUIRE(find_matching_attribute(mesh2, AttributeUsage::Normal).has_value());
}

TEST_CASE("io/msh", "[mesh][io][msh]")
{
    using namespace lagrange;
    using Scalar = double;
    using Index = uint32_t;

    SurfaceMesh<Scalar, Index> mesh;
    mesh.add_vertex({0, 0, 0});
    mesh.add_vertex({1, 0, 0});
    mesh.add_vertex({0, 1, 0});
    mesh.add_vertex({0, 0, 1});
    mesh.add_triangle(0, 2, 1);
    mesh.add_triangle(0, 3, 2);
    mesh.add_triangle(0, 1, 3);
    mesh.add_triangle(1, 2, 3);
    std::stringstream data;
    io::SaveOptions options;
    options.encoding = io::FileEncoding::Ascii;
    options.output_attributes = io::SaveOptions::OutputAttributes::SelectedOnly;

    SECTION("With vertex attribute")
    {
        std::string name = "index";
        auto id = mesh.template create_attribute<Scalar>(
            name,
            AttributeElement::Vertex,
            AttributeUsage::Scalar,
            1);
        auto& attr = mesh.template ref_attribute<Scalar>(id);
        auto buffer = attr.ref_all();
        buffer[0] = 0.;
        buffer[1] = 1.;
        buffer[2] = 2.;
        buffer[3] = 3.;

        options.selected_attributes.push_back(id);
    }

    SECTION("With facet attribute")
    {
        std::string name = "index";
        auto id = mesh.template create_attribute<Scalar>(
            name,
            AttributeElement::Facet,
            AttributeUsage::Scalar,
            1);
        auto& attr = mesh.template ref_attribute<Scalar>(id);
        auto buffer = attr.ref_all();
        buffer[0] = 0.;
        buffer[1] = 1.;
        buffer[2] = 2.;
        buffer[3] = 3.;

        options.selected_attributes.push_back(id);
    }

    SECTION("With corner attribute")
    {
        std::string name = "id";
        auto id = mesh.template create_attribute<Scalar>(
            name,
            AttributeElement::Corner,
            AttributeUsage::Scalar,
            1);
        auto& attr = mesh.template ref_attribute<Scalar>(id);
        auto buffer = attr.ref_all();
        REQUIRE(buffer.size() == 12);
        for (auto i : range(buffer.size())) {
            buffer[i] = static_cast<Scalar>(i);
        }
        options.selected_attributes.push_back(id);
    }

    SECTION("With int data")
    {
        std::string name = "index";
        auto id = mesh.template create_attribute<int>(
            name,
            AttributeElement::Vertex,
            AttributeUsage::Scalar,
            1);
        auto& attr = mesh.template ref_attribute<int>(id);
        auto buffer = attr.ref_all();
        buffer[0] = 0;
        buffer[1] = 1;
        buffer[2] = 2;
        buffer[3] = 3;

        options.selected_attributes.push_back(id);
    }

    SECTION("Multiple UV sets")
    {
        std::string name = "uv_0";
        auto id = mesh.template create_attribute<Scalar>(
            name,
            AttributeElement::Vertex,
            AttributeUsage::UV,
            2);
        auto attr = attribute_matrix_ref<Scalar>(mesh, id);
        attr.setZero();
        options.selected_attributes.push_back(id);

        name = "uv_1";
        id = mesh.template create_attribute<Scalar>(
            name,
            AttributeElement::Vertex,
            AttributeUsage::UV,
            2);
        attr = attribute_matrix_ref<Scalar>(mesh, id);
        attr.setOnes();
        options.selected_attributes.push_back(id);
    }

    SECTION("With vertex, facet and corner normal")
    {
        auto id0 = compute_weighted_corner_normal(mesh);
        auto id1 = compute_facet_normal(mesh);
        auto id2 = compute_vertex_normal(mesh);
        options.selected_attributes.push_back(id0);
        options.selected_attributes.push_back(id1);
        options.selected_attributes.push_back(id2);
    }

    REQUIRE_NOTHROW(io::save_mesh_msh(data, mesh, options));
    auto mesh2 = io::load_mesh_msh<SurfaceMesh<Scalar, Index>>(data);
    testing::check_mesh(mesh2);
    testing::ensure_approx_equivalent_mesh(mesh, mesh2);
}

TEST_CASE("io/msh hybrid", "[mesh][io][msh]")
{
    using namespace lagrange;
    using Scalar = double;
    using Index = uint32_t;

    SurfaceMesh<Scalar, Index> mesh;
    mesh.add_vertex({0, 0, 0});
    mesh.add_vertex({1, 0, 0});
    mesh.add_vertex({1, 1, 0});
    mesh.add_vertex({0, 1, 0});
    mesh.add_vertex({2, 0, 0});
    mesh.add_vertex({2, 1, 0});

    // Runs of length 2, then 1: two lines, two triangles, one quad, two triangles, one line.
    mesh.add_polygon({0, 1});
    mesh.add_polygon({1, 4});
    mesh.add_triangle(0, 1, 2);
    mesh.add_triangle(1, 4, 2);
    mesh.add_quad(0, 1, 2, 3);
    mesh.add_triangle(0, 3, 2);
    mesh.add_triangle(1, 2, 5);
    mesh.add_polygon({4, 5});
    REQUIRE(mesh.is_hybrid());
    REQUIRE(mesh.get_num_facets() == 8);
    REQUIRE(mesh.get_num_corners() == 2 + 2 + 3 + 3 + 4 + 3 + 3 + 2);

    auto facet_id = mesh.template create_attribute<Scalar>(
        "facet_index",
        AttributeElement::Facet,
        AttributeUsage::Scalar,
        1);
    auto corner_id = mesh.template create_attribute<Scalar>(
        "corner_index",
        AttributeElement::Corner,
        AttributeUsage::Scalar,
        1);
    for (Index f = 0; f < mesh.get_num_facets(); ++f) {
        mesh.template ref_attribute<Scalar>(facet_id).ref(f) = static_cast<Scalar>(f);
    }
    for (Index c = 0; c < mesh.get_num_corners(); ++c) {
        mesh.template ref_attribute<Scalar>(corner_id).ref(c) = static_cast<Scalar>(c);
    }

    auto roundtrip = [&](io::FileEncoding encoding) {
        std::stringstream data;
        io::SaveOptions options;
        options.encoding = encoding;
        REQUIRE_NOTHROW(io::save_mesh_msh(data, mesh, options));
        if (encoding == io::FileEncoding::Ascii) {
            const std::string text = data.str();
            const auto nodes = msh_node_blocks(text);
            REQUIRE(nodes.size() == 2);
            REQUIRE(nodes[0].entity_dim == 1);
            REQUIRE(nodes[0].entity_tag == 1);
            REQUIRE(nodes[0].count == 4);
            REQUIRE(nodes[1].entity_dim == 2);
            REQUIRE(nodes[1].entity_tag == 1);
            REQUIRE(nodes[1].count == 2);
            const auto elements = msh_element_blocks(text);
            REQUIRE(elements.size() == 5);
            REQUIRE(elements[0].entity_dim == 1);
            REQUIRE(elements[0].type_or_parametric == 1);
            REQUIRE(elements[0].count == 2);
            REQUIRE(elements[1].entity_dim == 2);
            REQUIRE(elements[1].type_or_parametric == 2);
            REQUIRE(elements[1].count == 2);
            REQUIRE(elements[2].type_or_parametric == 3);
            REQUIRE(elements[2].count == 1);
            REQUIRE(elements[3].type_or_parametric == 2);
            REQUIRE(elements[3].count == 2);
            REQUIRE(elements[4].entity_dim == 1);
            REQUIRE(elements[4].type_or_parametric == 1);
            REQUIRE(elements[4].count == 1);
            data.clear();
            data.str(text);
        }

        auto mesh2 = io::load_mesh_msh<SurfaceMesh<Scalar, Index>>(data);
        testing::check_mesh(mesh2);
        REQUIRE(mesh2.get_num_vertices() == mesh.get_num_vertices());
        REQUIRE(mesh2.get_num_facets() == mesh.get_num_facets());
        REQUIRE(mesh2.get_num_corners() == mesh.get_num_corners());
        REQUIRE(mesh2.is_hybrid());
        for (Index f = 0; f < mesh.get_num_facets(); ++f) {
            REQUIRE(mesh2.get_facet_size(f) == mesh.get_facet_size(f));
            auto expected = mesh.get_facet_vertices(f);
            auto actual = mesh2.get_facet_vertices(f);
            REQUIRE(actual.size() == expected.size());
            for (size_t lv = 0; lv < expected.size(); ++lv) {
                REQUIRE(actual[lv] == expected[lv]);
            }
        }
        for (Index v = 0; v < mesh.get_num_vertices(); ++v) {
            auto expected = mesh.get_position(v);
            auto actual = mesh2.get_position(v);
            for (Index d = 0; d < mesh.get_dimension(); ++d) {
                REQUIRE_THAT(actual[d], Catch::Matchers::WithinAbs(expected[d], 1e-12));
            }
        }

        REQUIRE(mesh2.has_attribute("facet_index"));
        REQUIRE(mesh2.has_attribute("corner_index"));
        const auto& facets = mesh2.template get_attribute<Scalar>("facet_index");
        const auto& corners = mesh2.template get_attribute<Scalar>("corner_index");
        REQUIRE(facets.get_element_type() == AttributeElement::Facet);
        REQUIRE(corners.get_element_type() == AttributeElement::Corner);
        for (Index f = 0; f < mesh.get_num_facets(); ++f) {
            REQUIRE_THAT(facets.get(f), Catch::Matchers::WithinAbs(static_cast<Scalar>(f), 1e-12));
        }
        for (Index c = 0; c < mesh.get_num_corners(); ++c) {
            REQUIRE_THAT(corners.get(c), Catch::Matchers::WithinAbs(static_cast<Scalar>(c), 1e-12));
        }
    };

    SECTION("ascii")
    {
        roundtrip(io::FileEncoding::Ascii);
    }
    SECTION("binary")
    {
        roundtrip(io::FileEncoding::Binary);
    }
}

TEST_CASE("io/msh line mesh", "[mesh][io][msh]")
{
    using namespace lagrange;
    using Scalar = double;
    using Index = uint32_t;

    SurfaceMesh<Scalar, Index> mesh;
    mesh.add_vertex({0, 0, 0});
    mesh.add_vertex({1, 0, 0});
    mesh.add_vertex({1, 1, 0});
    mesh.add_polygon({0, 1});
    mesh.add_polygon({1, 2});
    REQUIRE(mesh.is_regular());
    REQUIRE(mesh.get_vertex_per_facet() == 2);

    std::stringstream data;
    io::SaveOptions options;
    options.encoding = io::FileEncoding::Ascii;
    REQUIRE_NOTHROW(io::save_mesh_msh(data, mesh, options));
    const auto nodes = msh_node_blocks(data.str());
    REQUIRE(nodes.size() == 1);
    REQUIRE(nodes[0].entity_dim == 1);
    REQUIRE(nodes[0].entity_tag == 1);
    REQUIRE(nodes[0].count == 3);
    const auto elements = msh_element_blocks(data.str());
    REQUIRE(elements.size() == 1);
    REQUIRE(elements[0].entity_dim == 1);
    REQUIRE(elements[0].type_or_parametric == 1);
    REQUIRE(elements[0].count == 2);

    auto mesh2 = io::load_mesh_msh<SurfaceMesh<Scalar, Index>>(data);
    REQUIRE(mesh2.get_num_vertices() == 3);
    REQUIRE(mesh2.get_num_facets() == 2);
    REQUIRE(mesh2.get_facet_size(0) == 2);
    REQUIRE(mesh2.get_facet_size(1) == 2);
    REQUIRE(mesh2.get_facet_vertex(0, 0) == 0);
    REQUIRE(mesh2.get_facet_vertex(0, 1) == 1);
    REQUIRE(mesh2.get_facet_vertex(1, 0) == 1);
    REQUIRE(mesh2.get_facet_vertex(1, 1) == 2);
}

TEST_CASE("io/msh rejects other facet sizes", "[mesh][io][msh]")
{
    using namespace lagrange;
    using Scalar = double;
    using Index = uint32_t;

    SurfaceMesh<Scalar, Index> mesh;
    mesh.add_vertex({0, 0, 0});
    mesh.add_vertex({1, 0, 0});
    mesh.add_vertex({1, 1, 0});
    mesh.add_vertex({0, 1, 0});
    mesh.add_vertex({0.5, 1.5, 0});
    mesh.add_polygon({0, 1, 2, 3, 4});

    std::stringstream data;
    REQUIRE_THROWS(io::save_mesh_msh(data, mesh));
}

TEST_CASE("io/msh triangulation keeps lines", "[mesh][io][msh]")
{
    using namespace lagrange;
    using Scalar = double;
    using Index = uint32_t;

    SurfaceMesh<Scalar, Index> mesh;
    mesh.add_vertex({0, 0, 0});
    mesh.add_vertex({1, 0, 0});
    mesh.add_vertex({1, 1, 0});
    mesh.add_vertex({0, 1, 0});
    mesh.add_polygon({0, 1});
    mesh.add_quad(0, 1, 2, 3);

    std::stringstream data;
    REQUIRE_NOTHROW(io::save_mesh_msh(data, mesh));

    io::LoadOptions options;
    options.triangulate = true;
    auto mesh2 = io::load_mesh_msh<SurfaceMesh<Scalar, Index>>(data, options);
    Index num_lines = 0;
    Index num_large = 0;
    for (Index f = 0; f < mesh2.get_num_facets(); ++f) {
        if (mesh2.get_facet_size(f) == 2) ++num_lines;
        if (mesh2.get_facet_size(f) > 3) ++num_large;
    }
    REQUIRE(num_lines == 1);
    REQUIRE(num_large == 0);
    REQUIRE(mesh2.get_num_facets() > 1);
}

TEST_CASE("io/msh resolves tags outside block order", "[mesh][io][msh]")
{
    using namespace lagrange;
    using Scalar = double;
    using Index = uint32_t;

    // Curve nodes precede surface nodes, and the line block precedes the triangle. Element-node
    // data is listed in tag order, which is not facet order.
    const std::string text = R"msh($MeshFormat
4.1 0 8
$EndMeshFormat
$Nodes
2 5 1 5
1 1 0 2
4
5
0 1 0
1 1 0
2 1 0 3
1
2
3
0 0 0
1 0 0
0 1 0
$EndNodes
$Elements
2 2 1 2
1 1 1 1
2 4 5
2 1 2 1
1 1 2 3
$EndElements
$ElementNodeData
1
"corner_index"
1
0
4
0 1 2 0
1 3 1 2 3
2 2 4 5
$EndElementNodeData
)msh";

    std::istringstream input(text);
    auto mesh = io::load_mesh_msh<SurfaceMesh<Scalar, Index>>(input);
    REQUIRE(mesh.get_num_vertices() == 5);
    REQUIRE(mesh.get_num_facets() == 2);
    REQUIRE(mesh.get_facet_size(0) == 2);
    REQUIRE(mesh.get_facet_vertex(0, 0) == 3);
    REQUIRE(mesh.get_facet_vertex(0, 1) == 4);
    REQUIRE(mesh.get_facet_size(1) == 3);
    REQUIRE(mesh.get_facet_vertex(1, 0) == 0);
    REQUIRE(mesh.get_facet_vertex(1, 1) == 1);
    REQUIRE(mesh.get_facet_vertex(1, 2) == 2);
    REQUIRE(mesh.get_position(3)[1] == 1);
    REQUIRE(mesh.get_position(4)[0] == 1);

    const auto& corners = mesh.template get_attribute<Scalar>("corner_index");
    REQUIRE(corners.get_element_type() == AttributeElement::Corner);
    REQUIRE(corners.get(mesh.get_facet_corner_begin(0)) == 4);
    REQUIRE(corners.get(mesh.get_facet_corner_begin(0) + 1) == 5);
    REQUIRE(corners.get(mesh.get_facet_corner_begin(1)) == 1);
    REQUIRE(corners.get(mesh.get_facet_corner_begin(1) + 1) == 2);
    REQUIRE(corners.get(mesh.get_facet_corner_begin(1) + 2) == 3);
}

TEST_CASE("io/msh retains unreferenced point-entity nodes", "[mesh][io][msh]")
{
    using namespace lagrange;
    using Scalar = double;
    using Index = uint32_t;

    const std::string text = R"msh($MeshFormat
4.1 0 8
$EndMeshFormat
$Nodes
1 3 1 3
0 1 0 3
1
2
3
0 0 0
1 0 0
2 3 4
$EndNodes
$Elements
1 1 1 1
1 1 1 1
1 1 2
$EndElements
)msh";

    std::istringstream input(text);
    auto mesh = io::load_mesh_msh<SurfaceMesh<Scalar, Index>>(input);
    REQUIRE(mesh.get_num_vertices() == 3);
    REQUIRE(mesh.get_num_facets() == 1);
    REQUIRE(mesh.get_facet_size(0) == 2);
    REQUIRE(mesh.get_facet_vertex(0, 0) == 0);
    REQUIRE(mesh.get_facet_vertex(0, 1) == 1);
    REQUIRE(mesh.get_position(1)[0] == 1);
    REQUIRE(mesh.get_position(2)[0] == 2);
    REQUIRE(mesh.get_position(2)[1] == 3);
    REQUIRE(mesh.get_position(2)[2] == 4);
}

TEST_CASE("io/msh parametric curve node uses entity stride", "[mesh][io][msh]")
{
    using namespace lagrange;
    using Scalar = double;
    using Index = uint32_t;

    // One parameter after xyz. A surface stride of 5 would read past this block.
    const std::string text = R"msh($MeshFormat
4.1 0 8
$EndMeshFormat
$Nodes
1 1 1 1
1 1 1 1
1
0 0 0 0.25
$EndNodes
$Elements
0 0 1 0
$EndElements
)msh";

    std::istringstream input(text);
    auto mesh = io::load_mesh_msh<SurfaceMesh<Scalar, Index>>(input);
    REQUIRE(mesh.get_num_vertices() == 1);
    REQUIRE(mesh.get_num_facets() == 0);
    REQUIRE(mesh.get_position(0)[0] == 0);
    REQUIRE(mesh.get_position(0)[1] == 0);
    REQUIRE(mesh.get_position(0)[2] == 0);
    REQUIRE_FALSE(mesh.has_attribute("texcoord"));
}

TEST_CASE("io/msh partial element-node data", "[mesh][io][msh]")
{
    using namespace lagrange;
    using Scalar = double;
    using Index = uint32_t;

    const std::string text = R"msh($MeshFormat
4.1 0 8
$EndMeshFormat
$Nodes
1 4 1 4
2 1 0 4
1
2
3
4
0 0 0
1 0 0
0 1 0
1 1 0
$EndNodes
$Elements
1 2 1 2
2 1 2 2
1 1 2 3
2 2 3 4
$EndElements
$ElementNodeData
1
"corner_index"
1
0
4
0 1 1 0
2 3 7 8 9
$EndElementNodeData
)msh";

    std::istringstream input(text);
    auto mesh = io::load_mesh_msh<SurfaceMesh<Scalar, Index>>(input);
    REQUIRE(mesh.get_num_facets() == 2);
    const auto& corners = mesh.template get_attribute<Scalar>("corner_index");
    REQUIRE(corners.get(mesh.get_facet_corner_begin(0)) == 0);
    REQUIRE(corners.get(mesh.get_facet_corner_begin(1)) == 7);
    REQUIRE(corners.get(mesh.get_facet_corner_begin(1) + 1) == 8);
    REQUIRE(corners.get(mesh.get_facet_corner_begin(1) + 2) == 9);
}

TEST_CASE("io/msh load_lines false drops lines", "[mesh][io][msh]")
{
    using namespace lagrange;
    using Scalar = double;
    using Index = uint32_t;

    SurfaceMesh<Scalar, Index> mesh;
    mesh.add_vertex({0, 0, 0});
    mesh.add_vertex({1, 0, 0});
    mesh.add_vertex({0, 1, 0});
    mesh.add_triangle(0, 1, 2);
    mesh.add_polygon({0, 1});

    std::stringstream data;
    REQUIRE_NOTHROW(io::save_mesh_msh(data, mesh));

    io::LoadOptions options;
    options.load_lines = false;
    auto mesh2 = io::load_mesh_msh<SurfaceMesh<Scalar, Index>>(data, options);
    REQUIRE(mesh2.get_num_vertices() == 3);
    REQUIRE(mesh2.get_num_facets() == 1);
    REQUIRE(mesh2.get_facet_size(0) == 3);

    const std::string point_line = R"msh($MeshFormat
4.1 0 8
$EndMeshFormat
$Nodes
1 2 1 2
0 1 0 2
1
2
0 0 0
1 0 0
$EndNodes
$Elements
1 1 1 1
1 1 1 1
1 1 2
$EndElements
)msh";
    std::istringstream point_input(point_line);
    auto points = io::load_mesh_msh<SurfaceMesh<Scalar, Index>>(point_input, options);
    REQUIRE(points.get_num_vertices() == 2);
    REQUIRE(points.get_num_facets() == 0);
}

TEST_CASE("io/msh stitch ignores boundary lines", "[mesh][io][msh]")
{
    using namespace lagrange;
    using Scalar = double;
    using Index = uint32_t;

    SurfaceMesh<Scalar, Index> mesh;
    mesh.add_vertex({0, 0, 0});
    mesh.add_vertex({1, 0, 0});
    mesh.add_vertex({0, 1, 0});
    mesh.add_vertex({0, 0, 0});
    mesh.add_vertex({1, 0, 0});
    mesh.add_vertex({1, 1, 0});
    mesh.add_triangle(0, 1, 2);
    mesh.add_triangle(3, 4, 5);
    mesh.add_polygon({0, 1});
    mesh.add_polygon({3, 4});

    std::stringstream data;
    REQUIRE_NOTHROW(io::save_mesh_msh(data, mesh));

    io::LoadOptions options;
    options.stitch_vertices = true;
    auto mesh2 = io::load_mesh_msh<SurfaceMesh<Scalar, Index>>(data, options);
    REQUIRE(mesh2.get_num_vertices() == 4);
    REQUIRE(mesh2.get_num_facets() == 4);
    Index num_lines = 0;
    for (Index f = 0; f < mesh2.get_num_facets(); ++f) {
        if (mesh2.get_facet_size(f) != 2) continue;
        ++num_lines;
        REQUIRE(mesh2.get_facet_vertex(f, 0) != mesh2.get_facet_vertex(f, 1));
    }
    REQUIRE(num_lines == 2);
}

TEST_CASE("io/msh stitch keeps line endpoint attributes", "[mesh][io][msh]")
{
    using namespace lagrange;
    using Scalar = double;
    using Index = uint32_t;

    SurfaceMesh<Scalar, Index> mesh;
    mesh.add_vertex({0, 0, 0});
    mesh.add_vertex({1, 0, 0});
    mesh.add_vertex({0, 1, 0});
    mesh.add_vertex({2, 0, 0});
    mesh.add_vertex({3, 0, 0});
    mesh.add_triangle(0, 1, 2);
    mesh.add_polygon({3, 4});

    auto id = mesh.template create_attribute<Scalar>(
        "endpoint",
        AttributeElement::Vertex,
        AttributeUsage::Scalar,
        1);
    auto values = mesh.template ref_attribute<Scalar>(id).ref_all();
    for (Index v = 0; v < mesh.get_num_vertices(); ++v) {
        values[v] = static_cast<Scalar>(10 * (v + 1));
    }

    std::stringstream data;
    REQUIRE_NOTHROW(io::save_mesh_msh(data, mesh));
    io::LoadOptions options;
    options.stitch_vertices = true;
    auto mesh2 = io::load_mesh_msh<SurfaceMesh<Scalar, Index>>(data, options);
    REQUIRE(mesh2.is_attribute_indexed("endpoint"));

    const auto& attr = mesh2.template get_indexed_attribute<Scalar>("endpoint");
    const auto attr_values = attr.values().get_all();
    const auto attr_indices = attr.indices().get_all();
    bool found_line = false;
    for (Index f = 0; f < mesh2.get_num_facets(); ++f) {
        if (mesh2.get_facet_size(f) != 2) continue;
        found_line = true;
        const Index c0 = mesh2.get_facet_corner_begin(f);
        REQUIRE(attr_values[attr_indices[c0]] == 40);
        REQUIRE(attr_values[attr_indices[c0 + 1]] == 50);
    }
    REQUIRE(found_line);
}
