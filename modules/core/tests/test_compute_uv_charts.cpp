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
#include <lagrange/testing/common.h>

#include <lagrange/compute_uv_charts.h>

using namespace lagrange;

TEST_CASE("compute_uv_charts", "[surface][utilities]")
{
    using Scalar = double;
    using Index = uint32_t;

    SurfaceMesh<Scalar, Index> mesh;
    mesh.add_vertex({0, 0, 0});
    mesh.add_vertex({1, 0, 0});
    mesh.add_vertex({0, 1, 0});
    mesh.add_vertex({1, 1, 0});
    mesh.add_triangle(0, 1, 2);
    mesh.add_triangle(1, 3, 2);

    SECTION("Single chart")
    {
        std::vector<Scalar> uv_values = {0, 0, 1, 0, 0, 1, 1, 1};
        std::vector<Index> uv_indices = {0, 1, 2, 1, 3, 2};

        mesh.template create_attribute<Scalar>(
            "uv",
            AttributeElement::Indexed,
            AttributeUsage::UV,
            2,
            {uv_values.data(), uv_values.size()},
            {uv_indices.data(), uv_indices.size()});

        auto num_charts = compute_uv_charts(mesh);
        REQUIRE(num_charts == 1);
    }

    SECTION("Two charts")
    {
        std::vector<Scalar> uv_values = {0, 0, 1, 0, 0, 1, 1, 0, 1, 1, 0, 1};
        std::vector<Index> uv_indices = {0, 1, 2, 3, 4, 5};

        mesh.template create_attribute<Scalar>(
            "uv",
            AttributeElement::Indexed,
            AttributeUsage::UV,
            2,
            {uv_values.data(), uv_values.size()},
            {uv_indices.data(), uv_indices.size()});

        auto num_charts = compute_uv_charts(mesh);
        REQUIRE(num_charts == 2);
    }
}
