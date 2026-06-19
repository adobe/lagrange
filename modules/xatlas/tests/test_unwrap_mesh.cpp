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
#include <lagrange/xatlas/unwrap_mesh.h>

#include <lagrange/IndexedAttribute.h>
#include <lagrange/SurfaceMesh.h>
#include <lagrange/testing/common.h>
#include <lagrange/utils/Error.h>

#include <algorithm>
#include <cmath>

namespace {

template <typename Scalar, typename Index>
lagrange::SurfaceMesh<Scalar, Index> make_two_triangle_quad()
{
    lagrange::SurfaceMesh<Scalar, Index> mesh;
    mesh.add_vertex({0, 0, 0});
    mesh.add_vertex({1, 0, 0});
    mesh.add_vertex({0, 1, 0});
    mesh.add_vertex({1, 1, 0});
    mesh.add_triangle(0, 1, 2);
    mesh.add_triangle(1, 3, 2);
    return mesh;
}

template <typename Scalar, typename Index>
lagrange::SurfaceMesh<Scalar, Index> make_cube()
{
    using Mesh = lagrange::SurfaceMesh<Scalar, Index>;
    Mesh mesh;
    mesh.add_vertex({0, 0, 0});
    mesh.add_vertex({1, 0, 0});
    mesh.add_vertex({1, 1, 0});
    mesh.add_vertex({0, 1, 0});
    mesh.add_vertex({0, 0, 1});
    mesh.add_vertex({1, 0, 1});
    mesh.add_vertex({1, 1, 1});
    mesh.add_vertex({0, 1, 1});
    // -Z, +Z, -Y, +Y, -X, +X
    mesh.add_triangle(0, 2, 1);
    mesh.add_triangle(0, 3, 2);
    mesh.add_triangle(4, 5, 6);
    mesh.add_triangle(4, 6, 7);
    mesh.add_triangle(0, 1, 5);
    mesh.add_triangle(0, 5, 4);
    mesh.add_triangle(2, 3, 7);
    mesh.add_triangle(2, 7, 6);
    mesh.add_triangle(0, 4, 7);
    mesh.add_triangle(0, 7, 3);
    mesh.add_triangle(1, 2, 6);
    mesh.add_triangle(1, 6, 5);
    return mesh;
}

} // namespace

using Scalar = float;
using Index = uint32_t;

using SurfaceMesh = lagrange::SurfaceMesh<Scalar, Index>;
using UnwrapOptions = lagrange::xatlas::UnwrapOptions;
using Error = lagrange::Error;

TEST_CASE("two triangles", "[xatlas][unwrap_mesh]")
{
    auto mesh = make_two_triangle_quad<Scalar, Index>();

    const UnwrapOptions options;
    auto result = lagrange::xatlas::unwrap_mesh(mesh, options);

    REQUIRE(result.get_num_vertices() == mesh.get_num_vertices());
    REQUIRE(result.get_num_facets() == mesh.get_num_facets());
    REQUIRE(result.has_attribute(options.output_uv_attribute_name));
    REQUIRE(result.is_attribute_indexed(options.output_uv_attribute_name));
    REQUIRE(result.template is_attribute_type<Scalar>(options.output_uv_attribute_name));

    const auto& uv =
        result.template get_indexed_attribute<Scalar>(options.output_uv_attribute_name);
    REQUIRE(uv.get_num_channels() == 2u);

    auto values = uv.values().get_all();
    for (Scalar v : values) {
        REQUIRE(std::isfinite(v));
        REQUIRE(v >= Scalar(-1e-4));
        REQUIRE(v <= Scalar(1) + Scalar(1e-4));
    }
}

TEST_CASE("cube", "[xatlas][unwrap_mesh]")
{
    auto mesh = make_cube<Scalar, Index>();

    UnwrapOptions options;
    options.output_uv_attribute_name = "uv";
    auto result = lagrange::xatlas::unwrap_mesh(mesh, options);

    REQUIRE(result.has_attribute("uv"));
    REQUIRE(result.is_attribute_indexed("uv"));
    REQUIRE(result.template is_attribute_type<Scalar>("uv"));
}

TEST_CASE("empty", "[xatlas][unwrap_mesh]")
{
    SurfaceMesh mesh;

    auto result = lagrange::xatlas::unwrap_mesh(mesh);

    REQUIRE(result.get_num_vertices() == 0);
    REQUIRE(result.get_num_facets() == 0);
}

TEST_CASE("rejects non-triangle mesh", "[xatlas][unwrap_mesh]")
{
    SurfaceMesh mesh;
    mesh.add_vertex({0, 0, 0});
    mesh.add_vertex({1, 0, 0});
    mesh.add_vertex({1, 1, 0});
    mesh.add_vertex({0, 1, 0});
    mesh.add_quad(0, 1, 2, 3);

    REQUIRE_THROWS_AS(lagrange::xatlas::unwrap_mesh(mesh), Error);
}

TEST_CASE("ErrorIfMultiple atlas policy", "[xatlas][unwrap_mesh]")
{
    auto mesh = make_two_triangle_quad<Scalar, Index>();

    // The mesh is small enough that it fits in a single atlas, so this should succeed.
    UnwrapOptions options;
    options.multi_atlas_policy = lagrange::xatlas::MultiAtlasPolicy::ErrorIfMultiple;
    options.packing.resolution = 1024;
    options.packing.texels_per_unit = 1.f;
    auto result = lagrange::xatlas::unwrap_mesh(mesh, options);

    REQUIRE(result.has_attribute(options.output_uv_attribute_name));
    REQUIRE(result.is_attribute_indexed(options.output_uv_attribute_name));
    REQUIRE(result.template is_attribute_type<Scalar>(options.output_uv_attribute_name));
}

TEST_CASE("writes atlas index attribute", "[xatlas][unwrap_mesh]")
{
    auto mesh = make_two_triangle_quad<Scalar, Index>();

    UnwrapOptions options;
    options.output_atlas_attribute_name = "uv_atlas";
    auto result = lagrange::xatlas::unwrap_mesh(mesh, options);

    REQUIRE(result.has_attribute(options.output_atlas_attribute_name));
    REQUIRE(!result.is_attribute_indexed(options.output_atlas_attribute_name));
    REQUIRE(result.template is_attribute_type<int32_t>(options.output_atlas_attribute_name));
}

TEST_CASE("writes chart index attribute", "[xatlas][unwrap_mesh]")
{
    auto mesh = make_two_triangle_quad<Scalar, Index>();

    UnwrapOptions options;
    options.output_chart_attribute_name = "uv_chart";
    auto result = lagrange::xatlas::unwrap_mesh(mesh, options);

    REQUIRE(result.has_attribute(options.output_chart_attribute_name));
    REQUIRE(!result.is_attribute_indexed(options.output_chart_attribute_name));
    REQUIRE(result.template is_attribute_type<int32_t>(options.output_chart_attribute_name));
}
