/*
 * Copyright 2022 Adobe. All rights reserved.
 * This file is licensed to you under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License. You may obtain a copy
 * of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under
 * the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
 * OF ANY KIND, either express or implied. See the License for the specific language
 * governing permissions and limitations under the License.
 */
#include <lagrange/IndexedAttribute.h>
#include <lagrange/Logger.h>
#include <lagrange/SurfaceMeshTypes.h>
#include <lagrange/io/load_mesh.h>
#include <lagrange/io/save_mesh.h>
#include <lagrange/utils/safe_cast.h>

#include <lagrange/testing/common.h>

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <lagrange/fs/filesystem.h>
#include <spdlog/fmt/fmt.h>
#include <catch2/catch.hpp>
#include <lagrange/utils/warnon.h>
// clang-format on

#include <fstream>
#include <random>

namespace fs = lagrange::fs;

namespace {

template <typename S, typename I>
void test_load_save()
{
    using namespace lagrange;
    using MeshType = SurfaceMesh<S, I>;
    using Index = typename MeshType::Index;

    std::mt19937 gen;
    std::uniform_real_distribution<S> dist(0, 1);

    for (int i = 0; i < 8; ++i) {
        fs::path filename = fmt::format("semi{}.obj", i + 1);
        fs::path input_path = lagrange::testing::get_data_path("open/core/tilings" / filename);
        REQUIRE(fs::exists(input_path));
        auto output = io::load_mesh_obj<MeshType>(input_path.string());
        auto mesh = std::move(output.mesh);
        logger().info(
            "Loaded tiling with {} vertices and {} facets",
            mesh.get_num_vertices(),
            mesh.get_num_facets());
        REQUIRE(mesh.is_hybrid());
        for (Index v = 0; v < mesh.get_num_vertices(); ++v) {
            mesh.ref_position(v)[2] = dist(gen);
        }
        {
            fs::ofstream output_stream(filename);
            io::save_mesh_obj(output_stream, mesh);
        }
    }

    std::array<std::pair<std::string, int>, 3> test_cases = {
        {{"hexagon", 6}, {"square", 4}, {"triangle", 3}}};
    for (auto kv : test_cases) {
        fs::path filename = fmt::format("{}.obj", kv.first);
        fs::path input_path = lagrange::testing::get_data_path("open/core/tilings" / filename);
        REQUIRE(fs::exists(input_path));
        auto output = io::load_mesh_obj<MeshType>(input_path.string());
        auto mesh = std::move(output.mesh);
        logger().info(
            "Loaded tiling with {} vertices and {} facets",
            mesh.get_num_vertices(),
            mesh.get_num_facets());
        REQUIRE(mesh.is_regular());
        REQUIRE(mesh.get_vertex_per_facet() == safe_cast<Index>(kv.second));
        for (Index v = 0; v < mesh.get_num_vertices(); ++v) {
            mesh.ref_position(v)[2] = dist(gen);
        }
        {
            fs::ofstream output_stream(filename);
            io::save_mesh_obj(output_stream, mesh);
        }
    }
}

template <typename S, typename I>
void test_benchmark_tiles()
{
    using namespace lagrange;
    using MeshType = SurfaceMesh<S, I>;
    using Index = typename MeshType::Index;

    BENCHMARK("Timings Tiles")
    {
        int n = 0;

        for (int i = 0; i < 8; ++i) {
            fs::path filename = fmt::format("semi{}.obj", i + 1);
            fs::path input_path = lagrange::testing::get_data_path("open/core/tilings" / filename);
            REQUIRE(fs::exists(input_path));
            auto output = io::load_mesh_obj<MeshType>(input_path.string());
            auto mesh = std::move(output.mesh);
            n += safe_cast<int>(mesh.get_num_vertices());
            REQUIRE(mesh.is_hybrid());
            {
                fs::ofstream output_stream(filename);
                io::save_mesh_obj(output_stream, mesh);
            }
        }

        std::array<std::pair<std::string, int>, 3> test_cases = {
            {{"hexagon", 6}, {"square", 4}, {"triangle", 3}}};
        for (auto kv : test_cases) {
            fs::path filename = fmt::format("{}.obj", kv.first);
            fs::path input_path = lagrange::testing::get_data_path("open/core/tilings" / filename);
            REQUIRE(fs::exists(input_path));
            auto output = io::load_mesh_obj<MeshType>(input_path.string());
            auto mesh = std::move(output.mesh);
            n += safe_cast<int>(mesh.get_num_vertices());
            REQUIRE(mesh.is_regular());
            REQUIRE(mesh.get_vertex_per_facet() == safe_cast<Index>(kv.second));
            {
                fs::ofstream output_stream(filename);
                io::save_mesh_obj(output_stream, mesh);
            }
        }

        return n;
    };
}

template <typename S, typename I>
void test_io_blub()
{
    using namespace lagrange;
    using MeshType = SurfaceMesh<S, I>;
    using Scalar = typename MeshType::Scalar;

    fs::path path = lagrange::testing::get_data_path("open/core/blub/blub.obj");
    REQUIRE(fs::exists(path));
    auto res = io::load_mesh_obj<MeshType>(path.string());
    REQUIRE(res.success);
    auto mesh = std::move(res.mesh);
    fs::ofstream output_stream("blub.obj");
    io::save_mesh_obj(output_stream, mesh);

    logger().info("Mesh #v {}, #f {}", mesh.get_num_vertices(), mesh.get_num_facets());
    auto& uv_attr = mesh.template get_indexed_attribute<Scalar>("uv");
    logger().info("Mesh #uv {}", uv_attr.values().get_num_elements());
}

template <typename S, typename I>
void test_benchmark_large()
{
    using namespace lagrange;
    using MeshType = SurfaceMesh<S, I>;

    BENCHMARK("Timings Large")
    {
        // TODO: Add this guy to our unit test data
        fs::path path = "/Users/jedumas/cloud/adobe/shared/mesh_processing/Modeler - "
                        "Qadremesher/SoylentGreen_FullRes.obj";
        auto mesh = std::move(io::load_mesh_obj<MeshType>(path.string()).mesh);
        logger().info("Mesh #v {}, #f {}", mesh.get_num_vertices(), mesh.get_num_facets());
        return mesh.get_num_vertices();
        // Uncomment to save the obj as well
        // fs::ofstream output_stream("out.obj");
        // io::save_mesh_obj(output_stream, mesh);
    };
}

} // namespace

TEST_CASE("Mesh IO: Load and Save", "[next]")
{
#define LA_X_test_load_save(_, S, I) test_load_save<S, I>();
    LA_SURFACE_MESH_X(test_load_save, 0)
}

TEST_CASE("Mesh IO: Benchmark Tiles", "[next][!benchmark]")
{
#define LA_X_test_benchmark_tiles(_, S, I) test_benchmark_tiles<S, I>();
    LA_SURFACE_MESH_X(test_benchmark_tiles, 0)
}

TEST_CASE("Mesh IO: Blub", "[next]")
{
#define LA_X_test_io_blub(_, S, I) test_io_blub<S, I>();
    LA_SURFACE_MESH_X(test_io_blub, 0)
}

TEST_CASE("Mesh IO: Benchmark Large", "[next][!benchmark]" LA_CORP_FLAG)
{
#define LA_X_test_benchmark_large(_, S, I) test_benchmark_large<S, I>();
    LA_SURFACE_MESH_X(test_benchmark_large, 0)
}
