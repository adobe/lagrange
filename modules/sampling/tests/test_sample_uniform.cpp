/*
 * Copyright 2023 Adobe. All rights reserved.
 * This file is licensed to you under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License. You may obtain a copy
 * of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under
 * the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
 * OF ANY KIND, either express or implied. See the License for the specific language
 * governing permissions and limitations under the License.
 */
#include <lagrange/sampling/sample_uniform.h>
#include <lagrange/testing/common.h>

#include <catch2/matchers/catch_matchers_floating_point.hpp>

TEST_CASE("sample_uniform", "[sampling][uniform]")
{
    using Scalar = double;
    using Index = uint32_t;

    lagrange::SurfaceMesh<Scalar, Index> mesh;
    mesh.add_vertex({0, 0, 0});
    mesh.add_vertex({1, 0, 0});
    mesh.add_vertex({1, 1, 0});
    mesh.add_vertex({0, 1, 0});
    mesh.add_triangle(0, 1, 2);
    mesh.add_triangle(0, 2, 3);

    SECTION("Without index")
    {
        const Index N = 256;
        lagrange::sampling::UniformSamplingOptions options;
        options.num_samples = N;
        auto result = lagrange::sampling::sample_uniform(mesh, options);
        const auto& samples = result.samples;
        for (size_t i = 0; i < N; i++) {
            REQUIRE(samples[i * 3] >= 0);
            REQUIRE(samples[i * 3] <= 1);
            REQUIRE(samples[i * 3 + 1] >= 0);
            REQUIRE(samples[i * 3 + 1] <= 1);
            REQUIRE(samples[i * 3 + 2] == 0);
        }
    }

    SECTION("With index")
    {
        const Index N = 256;
        lagrange::sampling::UniformSamplingOptions options;
        options.num_samples = N;
        auto result = lagrange::sampling::sample_uniform(mesh, options);
        const auto& samples = result.samples;
        const auto& sample_facet_indices = result.facet_indices;
        size_t sample_count_0 = 0;
        for (size_t i = 0; i < N; i++) {
            REQUIRE(samples[i * 3] >= 0);
            REQUIRE(samples[i * 3] <= 1);
            REQUIRE(samples[i * 3 + 1] >= 0);
            REQUIRE(samples[i * 3 + 1] <= 1);
            REQUIRE(samples[i * 3 + 2] == 0);
            REQUIRE(sample_facet_indices[i] < 2);
            if (sample_facet_indices[i] == 0) sample_count_0++;
        }
        REQUIRE_THAT(double(sample_count_0) / double(N), Catch::Matchers::WithinAbs(0.5, 0.1));
    }
}
