/*
 * Copyright 2024 Adobe. All rights reserved.
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
#include <lagrange/compute_greedy_coloring.h>
#include <lagrange/testing/create_test_mesh.h>
#include <lagrange/utils/build.h>

#include <lagrange/Logger.h>
#include <lagrange/utils/fmt_eigen.h>
#include <lagrange/views.h>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("compute_greedy_coloring", "[core][coloring]")
{
    using Scalar = double;
    using Index = uint32_t;

    SECTION("facet coloring")
    {
        auto mesh = lagrange::testing::create_test_cube<Scalar, Index>();
        auto color_id = lagrange::compute_greedy_coloring(mesh);
        auto& color_attr = mesh.get_attribute<Index>(color_id);
        REQUIRE(color_attr.get_element_type() == lagrange::AttributeElement::Facet);
        auto colors = matrix_view(color_attr);
        Eigen::VectorX<Index> expected(mesh.get_num_facets());
        expected << 1, 0, 1, 0, 3, 6, 5, 3, 2, 6, 6, 0;
        CAPTURE(colors);
        CHECK(colors.maxCoeff() + 1 <= 7);
        CHECK((colors.array() == expected.array()).all());
    }

    SECTION("vertex coloring")
    {
        auto mesh = lagrange::testing::create_test_cube<Scalar, Index>();
        lagrange::GreedyColoringOptions options;
        options.element_type = lagrange::AttributeElement::Vertex;
        auto color_id = lagrange::compute_greedy_coloring(mesh, options);
        auto& color_attr = mesh.get_attribute<Index>(color_id);
        REQUIRE(color_attr.get_element_type() == lagrange::AttributeElement::Vertex);
        auto colors = matrix_view(color_attr);
        Eigen::VectorX<Index> expected(mesh.get_num_vertices());
        expected << 1, 0, 5, 6, 6, 5, 2, 0;
        CAPTURE(colors);
        CHECK(colors.maxCoeff() + 1 <= 7);
        CHECK((colors.array() == expected.array()).all());
    }
}
