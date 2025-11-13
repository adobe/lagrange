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
#include <lagrange/attribute_names.h>
#include <lagrange/fs/filesystem.h>
#include <lagrange/io/load_mesh.h>
#include <lagrange/io/save_mesh.h>
#include <lagrange/testing/common.h>
#include <lagrange/utils/safe_cast.h>

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <spdlog/fmt/fmt.h>
#include <catch2/catch_test_macros.hpp>
#include <catch2/benchmark/catch_benchmark.hpp>
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
        auto mesh = lagrange::testing::load_surface_mesh<S, I>("open/core/tilings" / filename);
        logger().info(
            "Loaded tiling with {} vertices and {} facets",
            mesh.get_num_vertices(),
            mesh.get_num_facets());
        REQUIRE(mesh.is_hybrid());
        for (Index v = 0; v < mesh.get_num_vertices(); ++v) {
            mesh.ref_position(v)[2] = dist(gen);
        }
        fs::path output_path = lagrange::testing::get_test_output_path("test_io" / filename);
        io::save_mesh(output_path, mesh);
    }

    std::array<std::pair<std::string, int>, 3> test_cases = {
        {{"hexagon", 6}, {"square", 4}, {"triangle", 3}}};
    for (auto kv : test_cases) {
        fs::path filename = fmt::format("{}.obj", kv.first);
        auto mesh = lagrange::testing::load_surface_mesh<S, I>("open/core/tilings" / filename);
        logger().info(
            "Loaded tiling with {} vertices and {} facets",
            mesh.get_num_vertices(),
            mesh.get_num_facets());
        REQUIRE(mesh.is_regular());
        REQUIRE(mesh.get_vertex_per_facet() == safe_cast<Index>(kv.second));
        for (Index v = 0; v < mesh.get_num_vertices(); ++v) {
            mesh.ref_position(v)[2] = dist(gen);
        }
        fs::path output_path = lagrange::testing::get_test_output_path("test_io" / filename);
        io::save_mesh(output_path, mesh);
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
            auto mesh = lagrange::testing::load_surface_mesh<S, I>("open/core/tilings" / filename);
            n += safe_cast<int>(mesh.get_num_vertices());
            REQUIRE(mesh.is_hybrid());
            fs::path output_path = lagrange::testing::get_test_output_path("test_io" / filename);
            io::save_mesh(output_path, mesh);
        }

        std::array<std::pair<std::string, int>, 3> test_cases = {
            {{"hexagon", 6}, {"square", 4}, {"triangle", 3}}};
        for (auto kv : test_cases) {
            fs::path filename = fmt::format("{}.obj", kv.first);
            auto mesh = lagrange::testing::load_surface_mesh<S, I>("open/core/tilings" / filename);
            n += safe_cast<int>(mesh.get_num_vertices());
            REQUIRE(mesh.is_regular());
            REQUIRE(mesh.get_vertex_per_facet() == safe_cast<Index>(kv.second));
            fs::path output_path = lagrange::testing::get_test_output_path("test_io" / filename);
            io::save_mesh(output_path, mesh);
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

    auto mesh = lagrange::testing::load_surface_mesh<S, I>("open/core/blub/blub.obj");
    fs::path output_path = lagrange::testing::get_test_output_path("test_io/blub.obj");
    io::save_mesh(output_path, mesh);

    logger().info("Mesh #v {}, #f {}", mesh.get_num_vertices(), mesh.get_num_facets());
    auto& uv_attr = mesh.template get_indexed_attribute<Scalar>(AttributeName::texcoord);
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
        auto mesh = io::load_mesh<MeshType>(path.string());
        logger().info("Mesh #v {}, #f {}", mesh.get_num_vertices(), mesh.get_num_facets());
        return mesh.get_num_vertices();
        // Uncomment to time obj save as well
        // fs::path output_path = lagrange::testing::get_test_output_path("test_io/large_out.obj");
        // fs::ofstream output_stream(output_path);
        // io::save_mesh_obj(output_stream, mesh);
    };
}

template <typename S, typename I>
void test_obj_indexing()
{
    using namespace lagrange;
    using MeshType = SurfaceMesh<S, I>;
    using Index = typename MeshType::Index;

    auto mesh = lagrange::testing::load_surface_mesh<S, I>("open/core/index-test.obj");

    for (Index f = 0; f < mesh.get_num_facets(); ++f) {
        const Index first_corner = mesh.get_facet_corner_begin(f);
        const Index last_corner = mesh.get_facet_corner_end(f);

        bool all_zero = true;
        for (Index c = first_corner; c < last_corner; ++c) {
            Index vertexIndex = mesh.get_corner_vertex(c);
            all_zero = all_zero && (vertexIndex == 0);
        }

        // Incorrect mesh indexing during obj load will result in uninitialized facets.
        REQUIRE(!all_zero);
    }
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

TEST_CASE("Mesh IO: Index Test", "[next]")
{
#define LA_X_test_obj_indexing(_, S, I) test_obj_indexing<S, I>();
    LA_SURFACE_MESH_X(test_obj_indexing, 0)
}
