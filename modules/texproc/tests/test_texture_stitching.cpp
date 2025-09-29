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
////////////////////////////////////////////////////////////////////////////////

#include "../examples/io_helpers.h"

#include <lagrange/find_matching_attributes.h>
#include <lagrange/map_attribute.h>
#include <lagrange/texproc/texture_stitching.h>
#include <lagrange/views.h>

#include <lagrange/testing/common.h>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

namespace {

// TODO: We're being VERY generous with the tolerances here. Let's investigate the discrepancies later.
void require_approx_mdspan(View3Df a, View3Df b, float eps_rel = 5e-1f, float eps_abs = 5e-1f)
{
    REQUIRE(a.extent(0) == b.extent(0));
    REQUIRE(a.extent(1) == b.extent(1));
    REQUIRE(a.extent(2) == b.extent(2));
    for (size_t x = 0; x < a.extent(0); ++x) {
        for (size_t y = 0; y < a.extent(1); ++y) {
            for (size_t c = 0; c < a.extent(2); ++c) {
                REQUIRE_THAT(
                    a(x, y, c),
                    Catch::Matchers::WithinRel(b(x, y, c), eps_rel) ||
                        (Catch::Matchers::WithinAbs(0, eps_abs) &&
                         Catch::Matchers::WithinAbs(b(x, y, c), eps_abs)));
            }
        }
    }
}

} // namespace

TEST_CASE("texture stitching quad", "[texproc]")
{
    using Scalar = double;
    using Index = uint32_t;
    lagrange::SurfaceMesh<Scalar, Index> quad_mesh;
    quad_mesh.add_vertex({0, 0, 0});
    quad_mesh.add_vertex({1, 0, 0});
    quad_mesh.add_vertex({1, 1, 0});
    quad_mesh.add_vertex({0, 1, 0});
    quad_mesh.add_triangle(0, 1, 2);
    quad_mesh.add_triangle(0, 2, 3);
    quad_mesh.create_attribute<Scalar>(
        "uv",
        lagrange::AttributeElement::Vertex,
        2,
        lagrange::AttributeUsage::UV);
    lagrange::attribute_matrix_ref<Scalar>(quad_mesh, "uv") =
        vertex_view(quad_mesh).template leftCols<2>();
    lagrange::map_attribute_in_place(quad_mesh, "uv", lagrange::AttributeElement::Indexed);

    auto img = lagrange::image::experimental::create_image<float>(128, 128, 3);

    REQUIRE_NOTHROW(lagrange::texproc::texture_stitching(quad_mesh, img.to_mdspan()));
}

TEST_CASE("texture stitching cube", "[texproc]")
{
    using Scalar = double;
    using Index = uint32_t;

    auto mesh = lagrange::testing::load_surface_mesh<Scalar, Index>("open/core/simple/cube_with_uv.obj");
    auto img = lagrange::image::experimental::create_image<float>(128, 128, 3);

    REQUIRE_NOTHROW(lagrange::texproc::texture_stitching(mesh, img.to_mdspan()));
}

TEST_CASE("texture stitching", "[texproc]" LA_SLOW_DEBUG_FLAG)
{
    using Scalar = double;
    using Index = uint32_t;
    auto mesh = lagrange::testing::load_surface_mesh<Scalar, Index>("open/core/blub/blub.obj");
    auto img = load_image(lagrange::testing::get_data_path("open/core/blub/blub_diffuse.png"));
    lagrange::texproc::StitchingOptions options;

    SECTION("default")
    {
        lagrange::texproc::texture_stitching(mesh, img.to_mdspan(), options);
        // save_image("blub_stitched.exr", img);
        auto expected =
            load_image(lagrange::testing::get_data_path("open/texproc/blub_stitched.exr"));
        require_approx_mdspan(img.to_mdspan(), expected.to_mdspan());
    }

    SECTION("randomized")
    {
        options.__randomize = true;
        lagrange::texproc::texture_stitching(mesh, img.to_mdspan(), options);
        auto expected =
            load_image(lagrange::testing::get_data_path("open/texproc/blub_stitched.exr"));
        require_approx_mdspan(img.to_mdspan(), expected.to_mdspan());
    }
}
