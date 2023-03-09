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
