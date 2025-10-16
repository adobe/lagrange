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
#include <lagrange/texproc/texture_filtering.h>

#include <lagrange/testing/common.h>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

namespace {

// TODO: We're being VERY generous with the tolerances here. Let's investigate the discrepancies
// later.
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

// TODO: Also run in debug mode with 128x128 texture?
TEST_CASE("texture filtering", "[texproc][!mayfail]" LA_SLOW_DEBUG_FLAG)
{
    using Scalar = double;
    using Index = uint32_t;
    auto mesh = lagrange::testing::load_surface_mesh<Scalar, Index>("open/core/blub/blub.obj");
    auto img = load_image(lagrange::testing::get_data_path("open/texproc/blub_diffuse_64x64.png"));
    lagrange::texproc::FilteringOptions options;

    SECTION("smoothing")
    {
        options.gradient_scale = 0;
        lagrange::texproc::texture_filtering(mesh, img.to_mdspan(), options);
        save_image("blub_smooth.exr", img);
        auto expected =
            load_image(lagrange::testing::get_data_path("open/texproc/blub_smooth.exr"));
        require_approx_mdspan(img.to_mdspan(), expected.to_mdspan());
    }

    SECTION("sharpening")
    {
        options.gradient_scale = 5.;
        lagrange::texproc::texture_filtering(mesh, img.to_mdspan(), options);
        save_image("blub_sharp.exr", img);
        auto expected = load_image(lagrange::testing::get_data_path("open/texproc/blub_sharp.exr"));
        require_approx_mdspan(img.to_mdspan(), expected.to_mdspan());
    }
}
