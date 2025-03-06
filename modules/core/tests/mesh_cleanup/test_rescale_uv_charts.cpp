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
#include <lagrange/mesh_cleanup/rescale_uv_charts.h>
#include <lagrange/compute_uv_distortion.h>
#include <lagrange/views.h>

#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <vector>

TEST_CASE("rescale_uv_charts", "[surface][mesh_cleanup]")
{
    using namespace lagrange;
    using Scalar = double;
    using Index = uint32_t;

    SurfaceMesh<Scalar, Index> mesh;
    mesh.add_vertex({0, 0, 0});
    mesh.add_vertex({1, 0, 0});
    mesh.add_vertex({1, 1, 0});
    mesh.add_vertex({0, 1, 0});
    mesh.add_triangle(0, 1, 2);
    mesh.add_triangle(0, 2, 3);

    auto check_uv_distortion = [&](double expected_distortion, std::string_view uv_attribute_name) {
        UVDistortionOptions options;
        options.metric = DistortionMetric::AreaRatio;
        options.uv_attribute_name = uv_attribute_name;

        auto id = compute_uv_distortion(mesh, options);

        auto distortion = attribute_vector_view<Scalar>(mesh, id);

        REQUIRE_THAT(distortion.maxCoeff(), Catch::Matchers::WithinAbs(expected_distortion, 1e-6));
    };

    SECTION("No scaling") {
        std::vector<Scalar> uv_values = {0, 0, 1, 0, 1, 1, 0, 1};
        std::vector<Index> uv_indices = {0, 1, 2, 0, 2, 3};
        mesh.template create_attribute<Scalar>(
            "uv", AttributeElement::Indexed, AttributeUsage::UV, 2, 
            {uv_values.data(), uv_values.size()},
            {uv_indices.data(), uv_indices.size()});

        check_uv_distortion(1.0, "uv");
        rescale_uv_charts(mesh);
        check_uv_distortion(1.0, "uv");
    }

    SECTION("Stretch in U") {
        std::vector<Scalar> uv_values = {0, 0, 10, 0, 10, 1, 0, 1};
        std::vector<Index> uv_indices = {0, 1, 2, 0, 2, 3};
        mesh.template create_attribute<Scalar>(
            "uv", AttributeElement::Indexed, AttributeUsage::UV, 2, 
            {uv_values.data(), uv_values.size()},
            {uv_indices.data(), uv_indices.size()});

        check_uv_distortion(10.0, "uv");
        rescale_uv_charts(mesh);
        check_uv_distortion(1.0, "uv");
    }

    SECTION("Stretch in U and V") {
        std::vector<Scalar> uv_values = {0, 0, 10, 0, 10, 10, 0, 10};
        std::vector<Index> uv_indices = {0, 1, 2, 0, 2, 3};
        mesh.template create_attribute<Scalar>(
            "uv", AttributeElement::Indexed, AttributeUsage::UV, 2, 
            {uv_values.data(), uv_values.size()},
            {uv_indices.data(), uv_indices.size()});

        check_uv_distortion(100.0, "uv");
        rescale_uv_charts(mesh);
        check_uv_distortion(1.0, "uv");
    }
}
