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
#include <lagrange/filtering/attribute_smoothing.h>
#include <lagrange/testing/common.h>
#include <lagrange/views.h>

#include <catch2/matchers/catch_matchers_floating_point.hpp>

TEST_CASE("scalar_field_smoothing", "[filtering]")
{
    using Scalar = float;
    using Index = uint32_t;

    //  6    7    8
    //  +----+----+
    //  |  \ | /  |
    // 3+---4+----+5
    //  |  / | \  |
    //  +----+----+
    //  0    1    2
    //
    lagrange::SurfaceMesh<Scalar, Index> mesh;
    mesh.add_vertex({0, 0, 0});
    mesh.add_vertex({1, 0, 0});
    mesh.add_vertex({2, 0, 0});
    mesh.add_vertex({0, 1, 0});
    mesh.add_vertex({1, 1, 0});
    mesh.add_vertex({2, 1, 0});
    mesh.add_vertex({0, 2, 0});
    mesh.add_vertex({1, 2, 0});
    mesh.add_vertex({2, 2, 0});

    mesh.add_triangle(0, 4, 3);
    mesh.add_triangle(0, 1, 4);
    mesh.add_triangle(1, 2, 4);
    mesh.add_triangle(2, 5, 4);
    mesh.add_triangle(3, 4, 6);
    mesh.add_triangle(6, 4, 7);
    mesh.add_triangle(4, 5, 8);
    mesh.add_triangle(4, 8, 7);

    SECTION("Single channel")
    {
        std::vector<float> signal = {0, 1, 0, 0, 0, 0, 0, 0, 0};
        mesh.template create_attribute<float>(
            "signal",
            lagrange::AttributeElement::Vertex,
            lagrange::AttributeUsage::Scalar,
            1,
            {signal.data(), signal.size()});

        lagrange::filtering::AttributeSmoothingOptions options;
        options.normal_smoothing_weight = 0;
        options.curvature_weight = 0;
        lagrange::filtering::scalar_attribute_smoothing(mesh, "signal", options);

        auto smoothed_signal = lagrange::attribute_vector_view<float>(mesh, "signal");
        REQUIRE(smoothed_signal[0] > 0.f);
        REQUIRE(smoothed_signal[1] < 1.f);
        REQUIRE(smoothed_signal[2] > 0.f);
    }

    SECTION("Multiple channels")
    {
        std::vector<float> signal(18, 0);
        signal[0] = 1;
        signal[17] = 1;
        mesh.template create_attribute<float>(
            "signal",
            lagrange::AttributeElement::Vertex,
            lagrange::AttributeUsage::Vector,
            2,
            {signal.data(), signal.size()});

        lagrange::filtering::AttributeSmoothingOptions options;
        options.normal_smoothing_weight = 0;
        options.curvature_weight = 0;
        lagrange::filtering::scalar_attribute_smoothing(mesh, "signal", options);

        auto smoothed_signal = lagrange::attribute_matrix_view<float>(mesh, "signal");
        for (size_t i = 0; i < 9; i++) {
            // Mesh and the vertex indices are radially symmetric. Thus, smoothing should be
            // radially symmetric as well.
            REQUIRE_THAT(
                smoothed_signal(i, 0),
                Catch::Matchers::WithinAbs(smoothed_signal(8 - i, 1), 1e-4f));
        }
    }
}
