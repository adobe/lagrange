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
#include <lagrange/xatlas/repack_mesh.h>

#include <lagrange/IndexedAttribute.h>
#include <lagrange/SurfaceMesh.h>
#include <lagrange/testing/common.h>
#include <lagrange/utils/Error.h>

namespace {

template <typename Scalar, typename Index>
lagrange::SurfaceMesh<Scalar, Index> make_two_islands()
{
    lagrange::SurfaceMesh<Scalar, Index> mesh;
    // Island 1: triangle 0
    mesh.add_vertex({0, 0, 0});
    mesh.add_vertex({1, 0, 0});
    mesh.add_vertex({0, 1, 0});
    // Island 2: triangle 1
    mesh.add_vertex({0, 0, 1});
    mesh.add_vertex({1, 0, 1});
    mesh.add_vertex({0, 1, 1});
    mesh.add_triangle(0, 1, 2);
    mesh.add_triangle(3, 4, 5);

    std::vector<Scalar> uv_values = {0, 0, 0.4f, 0, 0, 0.4f, 0.6f, 0, 1, 0, 0.6f, 0.4f};
    std::vector<Index> uv_indices = {0, 1, 2, 3, 4, 5};
    mesh.template create_attribute<Scalar>(
        "uv",
        lagrange::AttributeElement::Indexed,
        lagrange::AttributeUsage::UV,
        2,
        {uv_values.data(), uv_values.size()},
        {uv_indices.data(), uv_indices.size()});
    return mesh;
}

} // namespace

using Scalar = float;
using Index = uint32_t;

using SurfaceMesh = lagrange::SurfaceMesh<Scalar, Index>;
using RepackOptions = lagrange::xatlas::RepackOptions;
using Error = lagrange::Error;

TEST_CASE("basic", "[xatlas][repack_mesh]")
{
    auto mesh = make_two_islands<Scalar, Index>();

    RepackOptions options;
    options.input_uv_attribute_name = "uv";
    options.output_uv_attribute_name = "uv2";

    auto result = lagrange::xatlas::repack_mesh(mesh, options);

    REQUIRE(result.has_attribute("uv2"));
    REQUIRE(result.is_attribute_indexed("uv2"));
    REQUIRE(result.template is_attribute_type<Scalar>("uv2"));
    REQUIRE(result.get_num_vertices() == mesh.get_num_vertices());
    REQUIRE(result.get_num_facets() == mesh.get_num_facets());
}

TEST_CASE("rejects missing attribute", "[xatlas][repack_mesh]")
{
    SurfaceMesh mesh;
    mesh.add_vertex({0, 0, 0});
    mesh.add_vertex({1, 0, 0});
    mesh.add_vertex({0, 1, 0});
    mesh.add_triangle(0, 1, 2);

    RepackOptions options;
    options.input_uv_attribute_name = "uv";
    REQUIRE_THROWS_AS(lagrange::xatlas::repack_mesh(mesh, options), Error);
}

TEST_CASE("new output attribute defaults to mesh Scalar type", "[xatlas][repack_mesh]")
{
    lagrange::SurfaceMesh<double, Index> mesh;
    mesh.add_vertex({0, 0, 0});
    mesh.add_vertex({1, 0, 0});
    mesh.add_vertex({0, 1, 0});
    mesh.add_triangle(0, 1, 2);

    std::vector<float> uv_values = {0, 0, 1, 0, 0, 1};
    std::vector<Index> uv_indices = {0, 1, 2};
    mesh.template create_attribute<float>(
        "uv_in",
        lagrange::AttributeElement::Indexed,
        lagrange::AttributeUsage::UV,
        2,
        {uv_values.data(), uv_values.size()},
        {uv_indices.data(), uv_indices.size()});
    REQUIRE(mesh.has_attribute("uv_in"));
    REQUIRE(mesh.is_attribute_indexed("uv_in"));
    REQUIRE(mesh.template is_attribute_type<float>("uv_in"));

    RepackOptions options;
    options.input_uv_attribute_name = "uv_in";
    options.output_uv_attribute_name = "uv_out";
    auto result = lagrange::xatlas::repack_mesh(mesh, options);
    REQUIRE(result.has_attribute("uv_out"));
    REQUIRE(result.is_attribute_indexed("uv_out"));
    REQUIRE(result.template is_attribute_type<double>("uv_out"));
}

TEST_CASE("existing output attribute type is preserved", "[xatlas][repack_mesh]")
{
    SurfaceMesh mesh;
    mesh.add_vertex({0, 0, 0});
    mesh.add_vertex({1, 0, 0});
    mesh.add_vertex({0, 1, 0});
    mesh.add_triangle(0, 1, 2);

    std::vector<Scalar> uv_values = {0, 0, 1, 0, 0, 1};
    std::vector<Index> uv_indices = {0, 1, 2};
    mesh.template create_attribute<Scalar>(
        "uv_in",
        lagrange::AttributeElement::Indexed,
        lagrange::AttributeUsage::UV,
        2,
        {uv_values.data(), uv_values.size()},
        {uv_indices.data(), uv_indices.size()});
    mesh.template create_attribute<double>(
        "uv_out",
        lagrange::AttributeElement::Indexed,
        lagrange::AttributeUsage::UV,
        2);
    REQUIRE(mesh.has_attribute("uv_in"));
    REQUIRE(mesh.is_attribute_indexed("uv_in"));
    REQUIRE(mesh.template is_attribute_type<Scalar>("uv_in"));
    REQUIRE(mesh.has_attribute("uv_out"));
    REQUIRE(mesh.is_attribute_indexed("uv_out"));
    REQUIRE(mesh.template is_attribute_type<double>("uv_out"));

    RepackOptions options;
    options.input_uv_attribute_name = "uv_in";
    options.output_uv_attribute_name = "uv_out";
    auto result = lagrange::xatlas::repack_mesh(mesh, options);
    REQUIRE(result.has_attribute("uv_out"));
    REQUIRE(result.is_attribute_indexed("uv_out"));
    REQUIRE(result.template is_attribute_type<double>("uv_out"));
}

TEST_CASE("rejects non-indexed attribute", "[xatlas][repack_mesh]")
{
    SurfaceMesh mesh;
    mesh.add_vertex({0, 0, 0});
    mesh.add_vertex({1, 0, 0});
    mesh.add_vertex({0, 1, 0});
    mesh.add_triangle(0, 1, 2);

    std::vector<Scalar> uv_values = {0, 0, 1, 0, 0, 1};
    mesh.create_attribute<Scalar>(
        "uv",
        lagrange::AttributeElement::Vertex,
        lagrange::AttributeUsage::UV,
        2,
        {uv_values.data(), uv_values.size()});

    RepackOptions options;
    options.input_uv_attribute_name = "uv";
    REQUIRE_THROWS_AS(lagrange::xatlas::repack_mesh(mesh, options), Error);
}

TEST_CASE("chart attribute pins islands to charts", "[xatlas][repack_mesh]")
{
    auto mesh = make_two_islands<Scalar, Index>();

    // One distinct chart id per island.
    std::vector<int32_t> charts = {0, 1};
    mesh.template create_attribute<int32_t>(
        "chart",
        lagrange::AttributeElement::Facet,
        lagrange::AttributeUsage::Scalar,
        1,
        {charts.data(), charts.size()});

    RepackOptions options;
    options.input_uv_attribute_name = "uv";
    options.input_chart_attribute_name = "chart";
    options.output_uv_attribute_name = "uv2";

    auto result = lagrange::xatlas::repack_mesh(mesh, options);

    REQUIRE(result.has_attribute("uv2"));
    REQUIRE(result.is_attribute_indexed("uv2"));
    REQUIRE(result.get_num_facets() == mesh.get_num_facets());
}

TEST_CASE("rejects missing chart attribute", "[xatlas][repack_mesh]")
{
    auto mesh = make_two_islands<Scalar, Index>();

    RepackOptions options;
    options.input_uv_attribute_name = "uv";
    options.input_chart_attribute_name = "chart";
    REQUIRE_THROWS_AS(lagrange::xatlas::repack_mesh(mesh, options), Error);
}

TEST_CASE("rejects indexed chart attribute", "[xatlas][repack_mesh]")
{
    auto mesh = make_two_islands<Scalar, Index>();

    std::vector<int32_t> chart_values = {0, 1};
    std::vector<Index> chart_indices = {0, 0, 0, 1, 1, 1};
    mesh.template create_attribute<int32_t>(
        "chart",
        lagrange::AttributeElement::Indexed,
        lagrange::AttributeUsage::Scalar,
        1,
        {chart_values.data(), chart_values.size()},
        {chart_indices.data(), chart_indices.size()});

    RepackOptions options;
    options.input_uv_attribute_name = "uv";
    options.input_chart_attribute_name = "chart";
    REQUIRE_THROWS_AS(lagrange::xatlas::repack_mesh(mesh, options), Error);
}

TEST_CASE("rejects non-integer chart attribute", "[xatlas][repack_mesh]")
{
    auto mesh = make_two_islands<Scalar, Index>();

    std::vector<float> charts = {0.f, 1.f};
    mesh.template create_attribute<float>(
        "chart",
        lagrange::AttributeElement::Facet,
        lagrange::AttributeUsage::Scalar,
        1,
        {charts.data(), charts.size()});

    RepackOptions options;
    options.input_uv_attribute_name = "uv";
    options.input_chart_attribute_name = "chart";
    REQUIRE_THROWS_AS(lagrange::xatlas::repack_mesh(mesh, options), Error);
}
