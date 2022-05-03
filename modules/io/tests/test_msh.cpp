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
#include <lagrange/common.h>
#include <lagrange/testing/common.h>

#include <lagrange/Attribute.h>
#include <lagrange/io/load_mesh_msh.h>
#include <lagrange/io/save_mesh_msh.h>
#include <lagrange/utils/range.h>
#include <lagrange/foreach_attribute.h>

#include <sstream>


TEST_CASE("io/msh", "[mesh][io][msh]")
{
    using namespace lagrange;
    using Scalar = double;
    using Index = uint32_t;

    auto assert_same_attribute = [](const auto& attr1, const auto& attr2) {
        auto buffer1 = attr1.get_all();
        auto buffer2 = attr2.get_all();

        REQUIRE(buffer1.size() == buffer2.size());
        for (size_t i = 0; i < buffer1.size(); i++) {
            REQUIRE(buffer1[i] == buffer2[i]);
        }
    };

    auto assert_same_vertices = [&](const auto& m1, const auto& m2) {
        REQUIRE(m1.get_num_vertices() == m2.get_num_vertices());
        auto& vertices1 = m1.get_vertex_to_position();
        auto& vertices2 = m2.get_vertex_to_position();
        assert_same_attribute(vertices1, vertices2);
    };

    auto assert_same_facets = [&](const auto& m1, const auto& m2) {
        REQUIRE(m1.get_num_facets() == m2.get_num_facets());

        const auto num_facets = m1.get_num_facets();
        for (auto fid : range(num_facets)) {
            const auto f1 = m1.get_facet_vertices(fid);
            const auto f2 = m1.get_facet_vertices(fid);

            REQUIRE(f1.size() == f2.size());
            for (auto i : range(f1.size())) {
                REQUIRE(f1[i] == f2[i]);
            }
        }
    };

    auto assert_same = [&](const auto& m1, const auto& m2) {
        assert_same_vertices(m1, m2);
        assert_same_facets(m1, m2);

        m1.seq_foreach_attribute_id([&](AttributeId id) {
            if (!m1.template is_attribute_type<Scalar>(id)) return;

            auto attr_name = m1.get_attribute_name(id);
            REQUIRE(m2.has_attribute(attr_name));
            REQUIRE(m2.template is_attribute_type<Scalar>(attr_name));

            auto& attr1 = m1.template get_attribute<Scalar>(id);
            auto& attr2 = m2.template get_attribute<Scalar>(attr_name);
            assert_same_attribute(attr1, attr2);
        });
    };

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
    io::MshSaverOptions options;
    options.binary = false;

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

        options.attr_ids.push_back(id);
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

        options.attr_ids.push_back(id);
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
        options.attr_ids.push_back(id);
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

        options.attr_ids.push_back(id);
    }

    REQUIRE_NOTHROW(io::save_mesh_msh(data, mesh, options));
    auto mesh2 = io::load_mesh_msh<SurfaceMesh<Scalar, Index>>(data);
    assert_same(mesh, mesh2);
}
