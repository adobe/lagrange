/*
 * Copyright 2020 Adobe. All rights reserved.
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
#include <lagrange/compute_facet_area.h>
#include <lagrange/sampling/random_sample_density_blue_noise.h>
#include <lagrange/sampling/random_sample_uniform.h>

#include <lagrange/common.h>
#include <lagrange/create_mesh.h>

#include <lagrange/testing/common.h>
#include <catch2/benchmark/catch_benchmark.hpp>
////////////////////////////////////////////////////////////////////////////////

auto create_quad_2D()
{
    using MeshType = lagrange::TriangleMesh2D;

    MeshType::VertexArray vertices(4, 2);
    vertices << -1, -1, 1, -1, 1, 1, -1, 1;

    MeshType::FacetArray facets(2, 3);
    facets << 0, 1, 2, 0, 2, 3;

    auto mesh = lagrange::create_mesh(vertices, facets);

    lagrange::compute_facet_area(*mesh);

    return mesh;
}

TEST_CASE("Uniform sampling on single quad", "[sampling]")
{
    using MeshType = lagrange::TriangleMesh3D;
    using Index = MeshType::Index;

    const auto input = lagrange::create_quad(false);
    REQUIRE(input);
    REQUIRE(input->has_facet_attribute("area"));

    SECTION("Without index")
    {
        std::mt19937 rng;
        const auto ret = lagrange::sampling::random_sample_uniform(*input, 256, rng);

        const auto& cloud = std::get<0>(ret);
        REQUIRE(cloud.cols() == 3);
        REQUIRE(cloud.rows() == 256);
        for (Index kk = 0; kk < cloud.rows(); kk++) {
            REQUIRE(cloud(kk, 0) >= -1);
            REQUIRE(cloud(kk, 0) >= -1);
            REQUIRE(cloud(kk, 1) >= -1);
            REQUIRE(cloud(kk, 1) <= 1);
            REQUIRE(cloud(kk, 2) == 0);
        }

        const auto& indices = std::get<1>(ret);
        REQUIRE(indices.size() == 0);
    }

    SECTION("With facet indices")
    {
        std::mt19937 rng;
        const auto ret = lagrange::sampling::random_sample_uniform(*input, 256, rng, true);

        const auto& cloud = std::get<0>(ret);
        REQUIRE(cloud.cols() == 3);
        REQUIRE(cloud.rows() == 256);
        for (Index kk = 0; kk < cloud.rows(); kk++) {
            REQUIRE(cloud(kk, 0) >= -1);
            REQUIRE(cloud(kk, 0) >= -1);
            REQUIRE(cloud(kk, 1) >= -1);
            REQUIRE(cloud(kk, 1) <= 1);
            REQUIRE(cloud(kk, 2) == 0);
        }

        const auto& indices = std::get<1>(ret);
        REQUIRE(indices.cols() == 1);
        REQUIRE(indices.rows() == 256);
        for (Index kk = 0; kk < indices.rows(); kk++) {
            REQUIRE(indices(kk) >= 0);
            REQUIRE(indices(kk) < input->get_num_facets());
        }
    }
}

TEST_CASE("Uniform sampling on single 2D quad", "[sampling][2d]")
{
    using MeshType = lagrange::TriangleMesh2D;
    using Index = MeshType::Index;

    const auto input = create_quad_2D();
    REQUIRE(input);
    REQUIRE(input->has_facet_attribute("area"));

    SECTION("Without index")
    {
        std::mt19937 rng;
        const auto ret = lagrange::sampling::random_sample_uniform(*input, 256, rng);

        const auto& cloud = std::get<0>(ret);
        REQUIRE(cloud.cols() == 2);
        REQUIRE(cloud.rows() == 256);
        for (Index kk = 0; kk < cloud.rows(); kk++) {
            REQUIRE(cloud(kk, 0) >= -1);
            REQUIRE(cloud(kk, 0) >= -1);
            REQUIRE(cloud(kk, 1) >= -1);
            REQUIRE(cloud(kk, 1) <= 1);
        }

        const auto& indices = std::get<1>(ret);
        REQUIRE(indices.size() == 0);
    }

    SECTION("With facet indices")
    {
        std::mt19937 rng;
        const auto ret = lagrange::sampling::random_sample_uniform(*input, 256, rng, true);

        const auto& cloud = std::get<0>(ret);
        REQUIRE(cloud.cols() == 2);
        REQUIRE(cloud.rows() == 256);
        for (Index kk = 0; kk < cloud.rows(); kk++) {
            REQUIRE(cloud(kk, 0) >= -1);
            REQUIRE(cloud(kk, 0) >= -1);
            REQUIRE(cloud(kk, 1) >= -1);
            REQUIRE(cloud(kk, 1) <= 1);
        }

        const auto& indices = std::get<1>(ret);
        REQUIRE(indices.cols() == 1);
        REQUIRE(indices.rows() == 256);
        for (Index kk = 0; kk < indices.rows(); kk++) {
            REQUIRE(indices(kk) >= 0);
            REQUIRE(indices(kk) < input->get_num_facets());
        }
    }
}

TEST_CASE("Blue noise custom density sampling on single quad", "[sampling]")
{
    using MeshType = lagrange::TriangleMesh3D;
    using Cloud = lagrange::Vertices3D;
    using Indices = Eigen::Matrix<MeshType::Index, Eigen::Dynamic, 1>;
    using Densities = MeshType::AttributeArray;
    using Index = MeshType::Index;

    const auto input = lagrange::create_quad(false);
    REQUIRE(input);
    REQUIRE(input->has_facet_attribute("area"));

    SECTION("Invalid density")
    {
        std::mt19937 rng;
        const auto ret = lagrange::sampling::random_sample_density_blue_noise(
            *input,
            256,
            [](const Cloud& cloud, const Indices& /*indices*/) -> Densities {
                return Densities::Constant(cloud.rows(), 1, -1);
            },
            rng);

        const auto& status = std::get<0>(ret);
        const auto& cloud = std::get<1>(ret);
        const auto& densities = std::get<2>(ret);
        REQUIRE(status == lagrange::sampling::DensityStatus::ContainsNegative);
        REQUIRE(cloud.size() == 0);
        REQUIRE(densities.size() == 0);
    }

    SECTION("Uniform blue noise")
    {
        std::mt19937 rng;
        const auto ret = lagrange::sampling::random_sample_density_blue_noise(
            *input,
            1024,
            [](const Cloud& cloud, const Indices& /*indices*/) -> Densities {
                return Densities::Constant(cloud.rows(), 1, 1);
            },
            rng);

        const auto& status = std::get<0>(ret);
        const auto& cloud = std::get<1>(ret);
        const auto& densities = std::get<2>(ret);
        REQUIRE(status == lagrange::sampling::DensityStatus::Valid);
        REQUIRE(cloud.rows() == 1024);
        REQUIRE(cloud.cols() == 3);
        REQUIRE(densities.rows() == 1024);
        REQUIRE(densities.cols() == 1);

        for (Index kk = 0; kk < cloud.rows(); kk++) {
            REQUIRE(cloud(kk, 0) >= -1);
            REQUIRE(cloud(kk, 0) >= -1);
            REQUIRE(cloud(kk, 1) >= -1);
            REQUIRE(cloud(kk, 1) <= 1);
            REQUIRE(cloud(kk, 2) == 0);
        }
    }
}

TEST_CASE("Blue noise custom density sampling on single 2D quad", "[sampling][2d]")
{
    using MeshType = lagrange::TriangleMesh2D;
    using Cloud = lagrange::Vertices2D;
    using Indices = Eigen::Matrix<MeshType::Index, Eigen::Dynamic, 1>;
    using Densities = MeshType::AttributeArray;
    using Index = MeshType::Index;

    const auto input = create_quad_2D();
    REQUIRE(input);
    REQUIRE(input->has_facet_attribute("area"));

    SECTION("Invalid density")
    {
        std::mt19937 rng;
        const auto ret = lagrange::sampling::random_sample_density_blue_noise(
            *input,
            256,
            [](const Cloud& cloud, const Indices& /*indices*/) -> Densities {
                return Densities::Constant(cloud.rows(), 1, -1);
            },
            rng);

        const auto& status = std::get<0>(ret);
        const auto& cloud = std::get<1>(ret);
        const auto& densities = std::get<2>(ret);
        REQUIRE(status == lagrange::sampling::DensityStatus::ContainsNegative);
        REQUIRE(cloud.size() == 0);
        REQUIRE(densities.size() == 0);
    }

    SECTION("Uniform blue noise")
    {
        std::mt19937 rng;
        const auto ret = lagrange::sampling::random_sample_density_blue_noise(
            *input,
            1024,
            [](const Cloud& cloud, const Indices& /*indices*/) -> Densities {
                return Densities::Constant(cloud.rows(), 1, 1);
            },
            rng);

        const auto& status = std::get<0>(ret);
        const auto& cloud = std::get<1>(ret);
        const auto& densities = std::get<2>(ret);
        REQUIRE(status == lagrange::sampling::DensityStatus::Valid);
        REQUIRE(cloud.rows() == 1024);
        REQUIRE(cloud.cols() == 2);
        REQUIRE(densities.rows() == 1024);
        REQUIRE(densities.cols() == 1);

        for (Index kk = 0; kk < cloud.rows(); kk++) {
            REQUIRE(cloud(kk, 0) >= -1);
            REQUIRE(cloud(kk, 0) >= -1);
            REQUIRE(cloud(kk, 1) >= -1);
            REQUIRE(cloud(kk, 1) <= 1);
        }
    }
}

TEST_CASE("Uniform sampling on single quad benchmark", "[!benchmark][sampling]")
{
    const auto input = lagrange::create_quad(true);

    BENCHMARK("2048v", seed)
    {
        std::mt19937 rng(seed);
        const auto ret = lagrange::sampling::random_sample_uniform(*input, 2 * 1024, rng);
        return ret;
    };

    BENCHMARK("16kv", seed)
    {
        std::mt19937 rng(seed);
        const auto ret = lagrange::sampling::random_sample_uniform(*input, 16 * 1024, rng);
        return ret;
    };
}


TEST_CASE("Uniform blue noise on single quad benchmark", "[!benchmark][sampling]")
{
    using MeshType = lagrange::TriangleMesh3D;
    using Cloud = lagrange::Vertices3D;
    using Indices = Eigen::Matrix<MeshType::Index, Eigen::Dynamic, 1>;
    using Densities = MeshType::AttributeArray;

    const auto input = lagrange::create_quad(false);
    REQUIRE(input);
    REQUIRE(input->has_facet_attribute("area"));

    BENCHMARK("512v", seed)
    {
        std::mt19937 rng(seed);
        const auto ret = lagrange::sampling::random_sample_density_blue_noise(
            *input,
            512,
            [](const Cloud& cloud, const Indices& /*indices*/) -> Densities {
                return Densities::Constant(cloud.rows(), 1, 1);
            },
            rng);
        return ret;
    };

    BENCHMARK("1024v", seed)
    {
        std::mt19937 rng(seed);
        const auto ret = lagrange::sampling::random_sample_density_blue_noise(
            *input,
            1024,
            [](const Cloud& cloud, const Indices& /*indices*/) -> Densities {
                return Densities::Constant(cloud.rows(), 1, 1);
            },
            rng);
        return ret;
    };

    BENCHMARK("2048v", seed)
    {
        std::mt19937 rng(seed);
        const auto ret = lagrange::sampling::random_sample_density_blue_noise(
            *input,
            2 * 1024,
            [](const Cloud& cloud, const Indices& /*indices*/) -> Densities {
                return Densities::Constant(cloud.rows(), 1, 1);
            },
            rng);
        return ret;
    };

    BENCHMARK("4096v", seed)
    {
        std::mt19937 rng(seed);
        const auto ret = lagrange::sampling::random_sample_density_blue_noise(
            *input,
            4 * 1024,
            [](const Cloud& cloud, const Indices& /*indices*/) -> Densities {
                return Densities::Constant(cloud.rows(), 1, 1);
            },
            rng);
        return ret;
    };
}
