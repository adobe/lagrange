/*
 * Copyright 2019 Adobe. All rights reserved.
 * This file is licensed to you under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License. You may obtain a copy
 * of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under
 * the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
 * OF ANY KIND, either express or implied. See the License for the specific language
 * governing permissions and limitations under the License.
 */
#include <lagrange/io/load_mesh_ply.h>
#include <lagrange/io/save_mesh_ply.h>
#include <lagrange/testing/check_mesh.h>
#include <lagrange/testing/common.h>
#include <lagrange/testing/create_test_mesh.h>
#include <lagrange/testing/equivalence_check.h>

#include <sstream>

TEST_CASE("load_ply", "[io][ply]")
{
    using namespace lagrange;
    auto mesh =
        io::load_mesh_ply<SurfaceMesh32d>(testing::get_data_path("open/subdivision/sphere.ply"));
    REQUIRE(mesh.get_num_vertices() == 42);
    REQUIRE(mesh.get_num_facets() == 80);
}

TEST_CASE("io/ply", "[io][ply]")
{
    using namespace lagrange;
    using Scalar = double;
    using Index = uint32_t;

    auto mesh = testing::create_test_sphere<Scalar, Index>();
    std::stringstream data;
    io::SaveOptions save_options;
    save_options.output_attributes = io::SaveOptions::OutputAttributes::All;
    save_options.attribute_conversion_policy =
        io::SaveOptions::AttributeConversionPolicy::ConvertAsNeeded;

    SECTION("Ascii")
    {
        save_options.encoding = io::FileEncoding::Ascii;
        REQUIRE_NOTHROW(io::save_mesh_ply(data, mesh, save_options));
        auto mesh2 = io::load_mesh_ply<SurfaceMesh<Scalar, Index>>(data);
        testing::check_mesh(mesh2);
        testing::ensure_approx_equivalent_mesh(mesh, mesh2);
    }

    SECTION("Binary")
    {
        save_options.encoding = io::FileEncoding::Binary;
        REQUIRE_NOTHROW(io::save_mesh_ply(data, mesh, save_options));
        auto mesh2 = io::load_mesh_ply<SurfaceMesh<Scalar, Index>>(data);
        testing::check_mesh(mesh2);
        testing::ensure_approx_equivalent_mesh(mesh, mesh2);
    }
}

TEST_CASE("io/ply multiple special attributes", "[io][ply]")
{
    using namespace lagrange;
    using Scalar = double;
    using Index = uint32_t;

    auto mesh = testing::create_test_sphere<Scalar, Index>();
    std::stringstream data;
    io::SaveOptions save_options;
    io::LoadOptions load_options;
    save_options.output_attributes = io::SaveOptions::OutputAttributes::All;
    save_options.attribute_conversion_policy =
        io::SaveOptions::AttributeConversionPolicy::ConvertAsNeeded;
    load_options.load_vertex_colors = true;

    const auto num_vertices = mesh.get_num_vertices();
    std::vector<float> uv_data(num_vertices * 2, 1);
    std::vector<uint32_t> color_data(num_vertices * 4, 2);
    std::vector<float> normal_data(num_vertices * 3, 3);
    mesh.template create_attribute<float>(
        "uv2",
        AttributeElement::Vertex,
        AttributeUsage::UV,
        2,
        {uv_data.data(), uv_data.size()});
    mesh.template create_attribute<uint32_t>(
        "color2",
        AttributeElement::Vertex,
        AttributeUsage::Color,
        4,
        {color_data.data(), color_data.size()});
    mesh.template create_attribute<float>(
        "normal2",
        AttributeElement::Vertex,
        AttributeUsage::Normal,
        3,
        {normal_data.data(), normal_data.size()});

    SECTION("Ascii")
    {
        save_options.encoding = io::FileEncoding::Ascii;
        REQUIRE_NOTHROW(io::save_mesh_ply(data, mesh, save_options));
        io::save_mesh_ply("test.ply", mesh, save_options);
        auto mesh2 = io::load_mesh_ply<SurfaceMesh<Scalar, Index>>(data, load_options);
        testing::check_mesh(mesh2);
        testing::ensure_approx_equivalent_mesh(mesh, mesh2);
    }

    SECTION("Binary")
    {
        save_options.encoding = io::FileEncoding::Binary;
        REQUIRE_NOTHROW(io::save_mesh_ply(data, mesh, save_options));
        auto mesh2 = io::load_mesh_ply<SurfaceMesh<Scalar, Index>>(data, load_options);
        testing::check_mesh(mesh2);
        testing::ensure_approx_equivalent_mesh(mesh, mesh2);
    }
}
