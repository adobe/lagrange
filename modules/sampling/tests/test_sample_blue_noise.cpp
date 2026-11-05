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
#include <lagrange/Attribute.h>
#include <lagrange/SurfaceMesh.h>
#include <lagrange/sampling/DensityError.h>
#include <lagrange/sampling/sample_blue_noise.h>
#include <lagrange/testing/common.h>
#include <lagrange/views.h>

#include <catch2/benchmark/catch_benchmark.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

namespace {

/// Create a unit quad mesh (two triangles forming a [0,1]^2 square in the XY plane).
lagrange::SurfaceMesh<double, uint32_t> create_unit_quad()
{
    lagrange::SurfaceMesh<double, uint32_t> mesh;
    mesh.add_vertex({0, 0, 0});
    mesh.add_vertex({1, 0, 0});
    mesh.add_vertex({1, 1, 0});
    mesh.add_vertex({0, 1, 0});
    mesh.add_triangle(0, 1, 2);
    mesh.add_triangle(0, 2, 3);
    return mesh;
}

} // namespace

TEST_CASE("sample_blue_noise: uniform density", "[sampling][blue_noise]")
{
    using Scalar = double;
    using Index = uint32_t;

    auto mesh = create_unit_quad();
    const size_t N = 256;

    lagrange::sampling::BlueNoiseSamplingOptions<Scalar, Index> options;
    options.num_samples = N;
    options.seed = 42;

    auto result = lagrange::sampling::sample_blue_noise(mesh, options);
    REQUIRE(result.facet_indices.size() == N);
    REQUIRE(result.samples.size() == N * 3);
    REQUIRE(result.densities.size() == N);

    for (size_t i = 0; i < N; ++i) {
        REQUIRE(result.samples[i * 3] >= -1e-6);
        REQUIRE(result.samples[i * 3] <= 1.0 + 1e-6);
        REQUIRE(result.samples[i * 3 + 1] >= -1e-6);
        REQUIRE(result.samples[i * 3 + 1] <= 1.0 + 1e-6);
        REQUIRE_THAT(result.samples[i * 3 + 2], Catch::Matchers::WithinAbs(0.0, 1e-10));
        REQUIRE(result.facet_indices[i] < 2);
        REQUIRE_THAT(result.densities[i], Catch::Matchers::WithinAbs(1.0, 1e-10));
    }
}

TEST_CASE("sample_blue_noise: per-facet attribute density", "[sampling][blue_noise]")
{
    using Scalar = double;
    using Index = uint32_t;

    auto mesh = create_unit_quad();
    const size_t N = 512;

    // Assign density 2.0 to facet 0 and 1.0 to facet 1.
    auto density_id =
        mesh.template create_attribute<Scalar>("facet_density", lagrange::AttributeElement::Facet);
    auto& density_attr = mesh.template ref_attribute<Scalar>(density_id);
    density_attr.ref(0) = 2.0;
    density_attr.ref(1) = 1.0;

    lagrange::sampling::BlueNoiseSamplingOptions<Scalar, Index> options;
    options.num_samples = N;
    options.seed = 42;
    options.density = "facet_density";

    auto result = lagrange::sampling::sample_blue_noise(mesh, options);
    REQUIRE(result.facet_indices.size() == N);
    REQUIRE(result.samples.size() == N * 3);
    REQUIRE(result.densities.size() == N);

    // Verify output density for each sample matches the per-facet attribute value.
    size_t count_f0 = 0;
    for (size_t i = 0; i < N; ++i) {
        REQUIRE(result.facet_indices[i] < 2);
        Scalar expected = (result.facet_indices[i] == 0) ? 2.0 : 1.0;
        REQUIRE_THAT(result.densities[i], Catch::Matchers::WithinAbs(expected, 1e-10));
        if (result.facet_indices[i] == 0) ++count_f0;
    }
    // With density ratio 2:1 and bias=0, facet 0 should get more samples.
    double ratio = static_cast<double>(count_f0) / static_cast<double>(N);
    REQUIRE(ratio > 0.5);
}

TEST_CASE("sample_blue_noise: per-vertex attribute density", "[sampling][blue_noise]")
{
    using Scalar = double;
    using Index = uint32_t;

    auto mesh = create_unit_quad();
    const size_t N = 512;

    // Assign per-vertex density: higher near vertex 2 (1,1,0).
    auto density_id = mesh.template create_attribute<Scalar>(
        "vertex_density",
        lagrange::AttributeElement::Vertex);
    auto& density_attr = mesh.template ref_attribute<Scalar>(density_id);
    density_attr.ref(0) = 1.0; // (0,0,0)
    density_attr.ref(1) = 1.0; // (1,0,0)
    density_attr.ref(2) = 5.0; // (1,1,0)
    density_attr.ref(3) = 1.0; // (0,1,0)

    lagrange::sampling::BlueNoiseSamplingOptions<Scalar, Index> options;
    options.num_samples = N;
    options.seed = 42;
    options.density = "vertex_density";

    auto result = lagrange::sampling::sample_blue_noise(mesh, options);
    REQUIRE(result.facet_indices.size() == N);
    REQUIRE(result.samples.size() == N * 3);
    REQUIRE(result.densities.size() == N);

    // Verify sample positions and interpolated density values.
    for (size_t i = 0; i < N; ++i) {
        Scalar x = result.samples[i * 3];
        Scalar y = result.samples[i * 3 + 1];
        REQUIRE(x >= -1e-6);
        REQUIRE(x <= 1.0 + 1e-6);
        REQUIRE(y >= -1e-6);
        REQUIRE(y <= 1.0 + 1e-6);
        // Output density should be within the attribute range [1, 5].
        REQUIRE(result.densities[i] >= 1.0 - 1e-6);
        REQUIRE(result.densities[i] <= 5.0 + 1e-6);
    }

    // Samples near vertex 2 (1,1,0) with density=5 should be denser than near vertex 0 (0,0,0)
    // with density=1. Count samples in upper-right vs lower-left quadrants.
    size_t count_upper_right = 0;
    size_t count_lower_left = 0;
    for (size_t i = 0; i < N; ++i) {
        Scalar x = result.samples[i * 3];
        Scalar y = result.samples[i * 3 + 1];
        if (x > 0.5 && y > 0.5) ++count_upper_right;
        if (x < 0.5 && y < 0.5) ++count_lower_left;
    }
    REQUIRE(count_upper_right > count_lower_left);
}

TEST_CASE("sample_blue_noise: callback density", "[sampling][blue_noise]")
{
    using Scalar = double;
    using Index = uint32_t;

    auto mesh = create_unit_quad();
    const size_t N = 256;

    lagrange::sampling::BlueNoiseSamplingOptions<Scalar, Index> options;
    options.num_samples = N;
    options.seed = 42;

    // Callback: density proportional to x-coordinate. density = 1 + 4*x, ranging from 1 to 5.
    options.density = [](lagrange::span<const Scalar> positions,
                         lagrange::span<const Index> facet_indices,
                         lagrange::span<Scalar> output_densities) {
        const Index dim = 3;
        for (size_t i = 0; i < facet_indices.size(); ++i) {
            Scalar x = positions[i * dim];
            output_densities[i] = 1.0 + 4.0 * x;
        }
    };

    auto result = lagrange::sampling::sample_blue_noise(mesh, options);
    REQUIRE(result.facet_indices.size() == N);
    REQUIRE(result.samples.size() == N * 3);
    REQUIRE(result.densities.size() == N);

    // Verify density values match the callback formula.
    for (size_t i = 0; i < N; ++i) {
        Scalar x = result.samples[i * 3];
        REQUIRE(x >= -1e-6);
        REQUIRE(x <= 1.0 + 1e-6);
        REQUIRE(result.samples[i * 3 + 1] >= -1e-6);
        REQUIRE(result.samples[i * 3 + 1] <= 1.0 + 1e-6);
        Scalar expected_density = 1.0 + 4.0 * x;
        REQUIRE_THAT(result.densities[i], Catch::Matchers::WithinAbs(expected_density, 1e-10));
    }

    // Higher density at x=1 means more samples on the right side.
    size_t count_right = 0;
    for (size_t i = 0; i < N; ++i) {
        if (result.samples[i * 3] > 0.5) ++count_right;
    }
    REQUIRE(count_right > N / 2);
}

TEST_CASE("sample_blue_noise: invalid density", "[sampling][blue_noise]")
{
    using Scalar = double;
    using Index = uint32_t;

    auto mesh = create_unit_quad();

    lagrange::sampling::BlueNoiseSamplingOptions<Scalar, Index> options;
    options.num_samples = 64;
    options.seed = 42;

    SECTION("negative density callback")
    {
        options.density = []([[maybe_unused]] lagrange::span<const Scalar> positions,
                             lagrange::span<const Index> facet_indices,
                             lagrange::span<Scalar> output_densities) {
            for (size_t i = 0; i < facet_indices.size(); ++i) {
                output_densities[i] = -1.0;
            }
        };

        REQUIRE_THROWS_AS(
            lagrange::sampling::sample_blue_noise(mesh, options),
            lagrange::sampling::DensityError);
    }

    SECTION("non-positive sum density callback")
    {
        options.density = []([[maybe_unused]] lagrange::span<const Scalar> positions,
                             lagrange::span<const Index> facet_indices,
                             lagrange::span<Scalar> output_densities) {
            for (size_t i = 0; i < facet_indices.size(); ++i) {
                output_densities[i] = 0.0;
            }
        };

        REQUIRE_THROWS_AS(
            lagrange::sampling::sample_blue_noise(mesh, options),
            lagrange::sampling::DensityError);
    }

    SECTION("nan density callback")
    {
        options.density = []([[maybe_unused]] lagrange::span<const Scalar> positions,
                             lagrange::span<const Index> facet_indices,
                             lagrange::span<Scalar> output_densities) {
            for (size_t i = 0; i < facet_indices.size(); ++i) {
                output_densities[i] = i ? 1.0 : std::numeric_limits<Scalar>::quiet_NaN();
            }
        };

        REQUIRE_THROWS_AS(
            lagrange::sampling::sample_blue_noise(mesh, options),
            lagrange::sampling::DensityError);
    }
}

TEST_CASE("sample_blue_noise: seed reproducibility", "[sampling][blue_noise]")
{
    using Scalar = double;
    using Index = uint32_t;

    auto mesh = create_unit_quad();
    const size_t N = 128;

    lagrange::sampling::BlueNoiseSamplingOptions<Scalar, Index> options;
    options.num_samples = N;
    options.seed = 123;

    auto result1 = lagrange::sampling::sample_blue_noise(mesh, options);
    auto result2 = lagrange::sampling::sample_blue_noise(mesh, options);

    REQUIRE(result1.samples == result2.samples);
    REQUIRE(result1.facet_indices == result2.facet_indices);
    REQUIRE(result1.densities == result2.densities);
}

TEST_CASE("sample_blue_noise: benchmark uniform", "[sampling][blue_noise][!benchmark]")
{
    using Scalar = double;
    using Index = uint32_t;

    auto mesh = lagrange::testing::load_surface_mesh<Scalar, Index>("open/core/dragon.obj");

    BENCHMARK("512 samples")
    {
        lagrange::sampling::BlueNoiseSamplingOptions<Scalar, Index> options;
        options.num_samples = 512;
        options.seed = 42;
        return lagrange::sampling::sample_blue_noise(mesh, options);
    };

    BENCHMARK("1024 samples")
    {
        lagrange::sampling::BlueNoiseSamplingOptions<Scalar, Index> options;
        options.num_samples = 1024;
        options.seed = 42;
        return lagrange::sampling::sample_blue_noise(mesh, options);
    };

    BENCHMARK("2048 samples")
    {
        lagrange::sampling::BlueNoiseSamplingOptions<Scalar, Index> options;
        options.num_samples = 2048;
        options.seed = 42;
        return lagrange::sampling::sample_blue_noise(mesh, options);
    };

    BENCHMARK("4096 samples")
    {
        lagrange::sampling::BlueNoiseSamplingOptions<Scalar, Index> options;
        options.num_samples = 4096;
        options.seed = 42;
        return lagrange::sampling::sample_blue_noise(mesh, options);
    };
}

TEST_CASE("sample_blue_noise: benchmark variable density", "[sampling][blue_noise][!benchmark]")
{
    using Scalar = double;
    using Index = uint32_t;

    auto mesh = lagrange::testing::load_surface_mesh<Scalar, Index>("open/core/dragon.obj");

    // Set per-vertex density: higher near the top of the mesh.
    auto density_id = mesh.template create_attribute<Scalar>(
        "vertex_density",
        lagrange::AttributeElement::Vertex);
    {
        auto vertices = lagrange::vertex_view(mesh);
        Scalar z_min = vertices.col(2).minCoeff();
        Scalar z_max = vertices.col(2).maxCoeff();
        auto& density_attr = mesh.template ref_attribute<Scalar>(density_id);
        for (Index i = 0; i < mesh.get_num_vertices(); ++i) {
            // Density based on z-coordinate: ranges from 1 (bottom) to 5 (top).
            density_attr.ref(i) = 1.0 + 4.0 * (vertices(i, 2) - z_min) / (z_max - z_min);
        }
    }

    BENCHMARK("512 samples")
    {
        lagrange::sampling::BlueNoiseSamplingOptions<Scalar, Index> options;
        options.num_samples = 512;
        options.seed = 42;
        options.density = "vertex_density";
        return lagrange::sampling::sample_blue_noise(mesh, options);
    };

    BENCHMARK("1024 samples")
    {
        lagrange::sampling::BlueNoiseSamplingOptions<Scalar, Index> options;
        options.num_samples = 1024;
        options.seed = 42;
        options.density = "vertex_density";
        return lagrange::sampling::sample_blue_noise(mesh, options);
    };

    BENCHMARK("2048 samples")
    {
        lagrange::sampling::BlueNoiseSamplingOptions<Scalar, Index> options;
        options.num_samples = 2048;
        options.seed = 42;
        options.density = "vertex_density";
        return lagrange::sampling::sample_blue_noise(mesh, options);
    };

    BENCHMARK("4096 samples")
    {
        lagrange::sampling::BlueNoiseSamplingOptions<Scalar, Index> options;
        options.num_samples = 4096;
        options.seed = 42;
        options.density = "vertex_density";
        return lagrange::sampling::sample_blue_noise(mesh, options);
    };
}
